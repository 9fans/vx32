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


/*
 * Replacements for libmemdraw routines.
 * (They've been underscored.)
 */
Memimage*
allocmemimage(Rectangle r, uint32 chan)
{
	return _allocmemimage(r, chan);
}

void
freememimage(Memimage *m)
{
	_freememimage(m);
}


int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int n;

	n = _cloadmemimage(i, r, data, ndata);
	return n;
}

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

	/* now can run memimagedraw on the in-memory bits */
	_memimagedraw(par);
}

void
memfillcolor(Memimage *m, uint32 val)
{
	_memfillcolor(m, val);
}

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int n;

	n = _loadmemimage(i, r, data, ndata);
	return n;
}

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _unloadmemimage(i, r, data, ndata);
}

uint32
pixelbits(Memimage *m, Point p)
{
	return _pixelbits(m, p);
}

void
getcolor(ulong i, ulong *r, ulong *g, ulong *b)
{
	ulong v;
	
	v = 0;
	*r = (v>>16)&0xFF;
	*g = (v>>8)&0xFF;
	*b = v&0xFF;
}

int
setcolor(ulong i, ulong r, ulong g, ulong b)
{
	/* no-op */
	return 0;
}


int
hwdraw(Memdrawparam *p)
{
	return 0;
}

char*
getsnarf(void)
{
	return nil;
}

void
putsnarf(char *data)
{
}

void
setmouse(Point p)
{
}

void
setcursor(struct Cursor *c)
{
}

void
flushmemscreen(Rectangle r)
{
}

uchar*
attachscreen(Rectangle *r, ulong *chan, int *depth,
	int *width, int *softscreen, void **X)
{
	if(conf.monitor)
		panic("no gui - must use -g");
	return nil;
}
