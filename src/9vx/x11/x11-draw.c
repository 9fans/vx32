#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "error.h"
#define Image IMAGE	/* kernel has its own Image */
#include <draw.h>
#include <memdraw.h>
#include <keyboard.h>
#include <cursor.h>
#include "screen.h"
#include "x11-inc.h"

/*
 * Allocate a Memimage with an optional pixmap backing on the X server.
 */
Memimage*
_xallocmemimage(Rectangle r, uint32 chan, int pixmap)
{
	int d, offset;
	Memimage *m;
	Xmem *xm;
	XImage *xi;

	m = _allocmemimage(r, chan);
	if (m == nil)
		return nil;
	if(chan != GREY1 && chan != _x.chan)
		return m;
	if(_x.display == 0)
		return m;

	/*
	 * For bootstrapping, don't bother storing 1x1 images
	 * on the X server.  Memimageinit needs to allocate these
	 * and we memimageinit before we do the rest of the X stuff.
	 * Of course, 1x1 images on the server are useless anyway.
	 */
	if(Dx(r)==1 && Dy(r)==1)
		return m;

	xm = mallocz(sizeof(Xmem), 1);
	if(xm == nil){
		iprint("mallocz failed\n");
		freememimage(m);
		return nil;
	}

	/*
	 * Allocate backing store.
	 */
	if(chan == GREY1)
		d = 1;
	else
		d = _x.depth;
	if(pixmap != PMundef)
		xm->pixmap = pixmap;
	else
		xm->pixmap = XCreatePixmap(_x.display, _x.drawable, Dx(r), Dy(r), d);

	/*
	 * We want to align pixels on word boundaries.
	 */
	if(m->depth == 24)
		offset = r.min.x&3;
	else
		offset = r.min.x&(31/m->depth);
	r.min.x -= offset;
	assert(wordsperline(r, m->depth) <= m->width);

	/*
	 * Wrap our data in an XImage structure.
	 */
	xi = XCreateImage(_x.display, _x.vis, d,
		ZPixmap, 0, (char*)m->data->bdata, Dx(r), Dy(r),
		32, m->width*sizeof(uint32));
	if(xi == nil){
		iprint("XCreateImage %R %d %d failed\n", r, m->width, m->depth);
		freememimage(m);
		if(xm->pixmap != pixmap)
			XFreePixmap(_x.display, xm->pixmap);
		return nil;
	}

	xm->xi = xi;
	xm->r = r;

	/*
	 * Set the XImage parameters so that it looks exactly like
	 * a Memimage -- we're using the same data.
	 */
	if(m->depth < 8 || m->depth == 24)
		xi->bitmap_unit = 8;
	else
		xi->bitmap_unit = m->depth;
	xi->byte_order = LSBFirst;
	xi->bitmap_bit_order = MSBFirst;
	xi->bitmap_pad = 32;
	XInitImage(xi);
	XFlush(_x.display);

	m->x = xm;
	return m;
}

/*
 * Replacements for libmemdraw routines.
 * (They've been underscored.)
 */
Memimage*
allocmemimage(Rectangle r, uint32 chan)
{
	return _xallocmemimage(r, chan, PMundef);
}

void
freememimage(Memimage *m)
{
	Xmem *xm;

	if(m == nil)
		return;

	xm = m->x;
	if(xm && m->data->ref == 1){
		if(xm->xi){
			xm->xi->data = nil;
			XFree(xm->xi);
		}
		XFreePixmap(_x.display, xm->pixmap);
		free(xm);
		m->x = nil;
	}
	_freememimage(m);
}


int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int n;

	n = _cloadmemimage(i, r, data, ndata);
	if(n > 0 && i->x)
		_xputxdata(i, r);
	return n;
}

static int xdraw(Memdrawparam*);

/*
 * The X acceleration doesn't fit into the standard hwaccel
 * model because we have the extra steps of pulling the image
 * data off the server and putting it back when we're done.
 */
void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp,
	Memimage *mask, Point mp, int op)
{
	Memdrawparam *par;

	if((par = _memimagedrawsetup(dst, r, src, sp, mask, mp, op)) == nil)
		return;

	/* only fetch dst data if we need it */
	if((par->state&(Simplemask|Fullmask)) != (Simplemask|Fullmask))
		_xgetxdata(par->dst, par->r);

	/* always fetch source and mask */
	_xgetxdata(par->src, par->sr);
	_xgetxdata(par->mask, par->mr);

	/* now can run memimagedraw on the in-memory bits */
	_memimagedraw(par);

	if(xdraw(par))
		return;

	/* put bits back on x server */
	_xputxdata(par->dst, par->r);
}

static int
xdraw(Memdrawparam *par)
{
	uint32 sdval;
	uint m, state;
	Memimage *src, *dst, *mask;
	Point dp, mp, sp;
	Rectangle r;
	Xmem *xdst, *xmask, *xsrc;
	XGC gc;

	if(par->dst->x == nil)
		return 0;

	dst   = par->dst;
	mask  = par->mask;
	r     = par->r;
	src   = par->src;
	state = par->state;

	/*
	 * If we have an opaque mask and source is one opaque pixel,
	 * we can convert to the destination format and just XFillRectangle.
	 */
	m = Simplesrc|Fullsrc|Simplemask|Fullmask;
	if((state&m) == m){
		_xfillcolor(dst, r, par->sdval);
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * If no source alpha and an opaque mask, we can just copy
	 * the source onto the destination.  If the channels are the
	 * same and the source is not replicated, XCopyArea works.
	 */
	m = Simplemask|Fullmask;
	if((state&(m|Replsrc))==m && src->chan==dst->chan && src->x){
		xdst = dst->x;
		xsrc = src->x;
		dp = subpt(r.min,       dst->r.min);
		sp = subpt(par->sr.min, src->r.min);
		gc = dst->chan==GREY1 ?  _x.gccopy0 : _x.gccopy;

		XCopyArea(_x.display, xsrc->pixmap, xdst->pixmap, gc,
			sp.x, sp.y, Dx(r), Dy(r), dp.x, dp.y);
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * If no source alpha, a 1-bit mask, and a simple source,
	 * we can copy through the mask onto the destination.
	 */
	if(dst->x && mask->x && !(mask->flags&Frepl)
	&& mask->chan==GREY1 && (state&Simplesrc)){
		xdst = dst->x;
		xmask = mask->x;
		sdval = par->sdval;

		dp = subpt(r.min, dst->r.min);
		mp = subpt(r.min, subpt(par->mr.min, mask->r.min));

		if(dst->chan == GREY1){
			gc = _x.gcsimplesrc0;
			if(_x.gcsimplesrc0color != sdval){
				XSetForeground(_x.display, gc, sdval);
				_x.gcsimplesrc0color = sdval;
			}
			if(_x.gcsimplesrc0pixmap != xmask->pixmap){
				XSetStipple(_x.display, gc, xmask->pixmap);
				_x.gcsimplesrc0pixmap = xmask->pixmap;
			}
		}else{
			/* this doesn't work on rob's mac?  */
			return 0;
			/* gc = _x.gcsimplesrc;
			if(dst->chan == CMAP8 && _x.usetable)
				sdval = _x.tox11[sdval];

			if(_x.gcsimplesrccolor != sdval){
				XSetForeground(_x.display, gc, sdval);
				_x.gcsimplesrccolor = sdval;
			}
			if(_x.gcsimplesrcpixmap != xmask->pixmap){
				XSetStipple(_x.display, gc, xmask->pixmap);
				_x.gcsimplesrcpixmap = xmask->pixmap;
			}
			*/
		}
		XSetTSOrigin(_x.display, gc, mp.x, mp.y);
		XFillRectangle(_x.display, xdst->pixmap, gc, dp.x, dp.y,
			Dx(r), Dy(r));
	/*	xdirtyxdata(dst, r); */
		return 1;
	}

	/*
	 * Can't accelerate.
	 */
	return 0;
}


void
memfillcolor(Memimage *m, uint32 val)
{
	_memfillcolor(m, val);
	if(m->x == nil)
		return;
	if((val & 0xFF) == 0xFF)	/* full alpha */
		_xfillcolor(m, m->r, _rgbatoimg(m, val));
	else
		_xputxdata(m, m->r);
}

void
_xfillcolor(Memimage *m, Rectangle r, uint32 v)
{
	Point p;
	Xmem *xm;
	XGC gc;
	
	xm = m->x;
	assert(xm != nil);

	/*
	 * Set up fill context appropriately.
	 */
	if(m->chan == GREY1){
		gc = _x.gcfill0;
		if(_x.gcfill0color != v){
			XSetForeground(_x.display, gc, v);
			_x.gcfill0color = v;
		}
	}else{
		if(m->chan == CMAP8 && _x.usetable)
			v = _x.tox11[v];
		gc = _x.gcfill;
		if(_x.gcfillcolor != v){
			XSetForeground(_x.display, gc, v);
			_x.gcfillcolor = v;
		}
	}

	/*
	 * XFillRectangle takes coordinates relative to image rectangle.
	 */
	p = subpt(r.min, m->r.min);
	XFillRectangle(_x.display, xm->pixmap, gc, p.x, p.y, Dx(r), Dy(r));
}

static void
addrect(Rectangle *rp, Rectangle r)
{
	if(rp->min.x >= rp->max.x)
		*rp = r;
	else
		combinerect(rp, r);
}

XImage*
_xgetxdata(Memimage *m, Rectangle r)
{
	int x, y;
	uchar *p;
	Point tp, xdelta, delta;
	Xmem *xm;
	
	xm = m->x;
	if(xm == nil)
		return nil;

	if(xm->dirty == 0)
		return xm->xi;

	abort();	/* should never call this now */

	r = xm->dirtyr;
	if(Dx(r)==0 || Dy(r)==0)
		return xm->xi;

	delta = subpt(r.min, m->r.min);

	tp = xm->r.min;	/* need temp for Digital UNIX */
	xdelta = subpt(r.min, tp);

	XGetSubImage(_x.display, xm->pixmap, delta.x, delta.y, Dx(r), Dy(r),
		AllPlanes, ZPixmap, xm->xi, xdelta.x, delta.y);

	if(_x.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = _x.toplan9[*p];
	}
	xm->dirty = 0;
	xm->dirtyr = Rect(0,0,0,0);
	return xm->xi;
}

void
_xputxdata(Memimage *m, Rectangle r)
{
	int offset, x, y;
	uchar *p;
	Point tp, xdelta, delta;
	Xmem *xm;
	XGC gc;
	XImage *xi;

	xm = m->x;
	if(xm == nil)
		return;

	xi = xm->xi;
	gc = m->chan==GREY1 ? _x.gccopy0 : _x.gccopy;
	if(m->depth == 24)
		offset = r.min.x & 3;
	else
		offset = r.min.x & (31/m->depth);

	delta = subpt(r.min, m->r.min);

	tp = xm->r.min;	/* need temporary on Digital UNIX */
	xdelta = subpt(r.min, tp);

	if(_x.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = _x.tox11[*p];
	}

	XPutImage(_x.display, xm->pixmap, gc, xi, xdelta.x, xdelta.y, delta.x, delta.y,
		Dx(r), Dy(r));
	
	if(_x.usetable && m->chan==CMAP8){
		for(y=r.min.y; y<r.max.y; y++)
		for(x=r.min.x, p=byteaddr(m, Pt(x,y)); x<r.max.x; x++, p++)
			*p = _x.toplan9[*p];
	}
}

void
_xdirtyxdata(Memimage *m, Rectangle r)
{
	Xmem *xm;

	xm = m->x;
	if(xm == nil)
		return;

	xm->dirty = 1;
	addrect(&xm->dirtyr, r);
}


int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int n;

	n = _loadmemimage(i, r, data, ndata);
	if(n > 0 && i->x)
		_xputxdata(i, r);
	return n;
}

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	if(i->x)
		_xgetxdata(i, r);
	return _unloadmemimage(i, r, data, ndata);
}

uint32
pixelbits(Memimage *m, Point p)
{
	if(m->x)
		_xgetxdata(m, Rect(p.x, p.y, p.x+1, p.y+1));
	return _pixelbits(m, p);
}
