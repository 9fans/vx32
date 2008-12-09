#include "u.h"
#include "lib.h"
#include "draw.h"
#include "memdraw.h"

Memimage*
allocmemimage(Rectangle r, uint32 chan)
{
	return _allocmemimage(r, chan);
}

void
freememimage(Memimage *i)
{
	_freememimage(i);
}

void
memfillcolor(Memimage *i, uint32 val)
{
	_memfillcolor(i, val);
}


int
cloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _cloadmemimage(i, r, data, ndata);
}

void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point sp, Memimage *mask, Point mp, int op)
{
	_memimagedraw(_memimagedrawsetup(dst, r, src, sp, mask, mp, op));
}

uint32
pixelbits(Memimage *m, Point p)
{
	return _pixelbits(m, p);
}

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _loadmemimage(i, r, data, ndata);
}

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	return _unloadmemimage(i, r, data, ndata);
}

