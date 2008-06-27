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
#include "mouse.h"
#include "x11-inc.h"

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

#define Mask MouseMask|ExposureMask|StructureNotifyMask|KeyPressMask|EnterWindowMask|LeaveWindowMask

Rectangle windowrect;
Rectangle screenrect;
int fullscreen;

/*
 * Handle an incoming X event.
 */
void
runxevent(XEvent *xev)
{
	KeySym k;
	static Mouse m;

#ifdef SHOWEVENT
	static int first = 1;
	if(first){
		dup(create("/tmp/devdraw.out", OWRITE, 0666), 1);
		setbuf(stdout, 0);
		first = 0;
	}
#endif

	if(xev == 0)
		return;

#ifdef SHOWEVENT
	print("\n");
	ShowEvent(xev);
#endif

	switch(xev->type){
	case Expose:
		drawqlock();
		_xexpose(xev);
		drawqunlock();
		break;
	
	case DestroyNotify:
		if(_xdestroy(xev))
			exit(0);
		break;

	case ConfigureNotify:
		drawqlock();
		if(_xconfigure(xev)){
			drawqunlock();
			_xreplacescreenimage();
		}else
			drawqunlock();
		break;

	case ButtonPress:
	case ButtonRelease:
	case MotionNotify:
		if(_xtoplan9mouse(xev, &m) < 0)
			return;
		mousetrack(m.xy.x, m.xy.y, m.buttons, m.msec);
		break;
	
	case KeyPress:
		XLookupString((XKeyEvent*)xev, NULL, 0, &k, NULL);
		if(k == XK_F11){
			fullscreen = !fullscreen;
		//TODO	_xmovewindow(fullscreen ? screenrect : windowrect);
			return;
		}
		_xtoplan9kbd(xev);
		break;
	
	case SelectionRequest:
		_xselect(xev);
		break;
	}
}

static void
_xproc(void *v)
{
	XEvent event;

	XSelectInput(_x.kmcon, _x.drawable, Mask);
	for(;;){
		XNextEvent(_x.kmcon, &event);
		runxevent(&event);
	}
}

uchar*
attachscreen(Rectangle *r, ulong *chan, int *depth,
	int *width, int *softscreen, void **X)
{
	Memimage *m;

	if(_x.screenimage == nil){
		_memimageinit();
		if(_xattach("9vx", nil) == nil)
			panic("cannot connect to X display: %r");
		kproc("*x11*", _xproc, nil);
	}
	m = _x.screenimage;
	*r = m->r;
	*chan = m->chan;
	*depth = m->depth;
	*width = m->width;
	*X = m->x;
	*softscreen = 1;
	return m->data->bdata;
}

int
hwdraw(Memdrawparam *p)
{
	return 0;
}

void
getcolor(ulong i, ulong *r, ulong *g, ulong *b)
{
	ulong v;
	
	v = cmap2rgb(i);
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

char*
getsnarf(void)
{
	return _xgetsnarf();
}

void
putsnarf(char *data)
{
	_xputsnarf(data);
}

void
setmouse(Point p)
{
	drawqlock();
	_xmoveto(p);
	drawqunlock();
}
