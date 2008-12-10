#define Colormap	XColormap
#define Cursor		XCursor
#define Display		XDisplay
#define Drawable	XDrawable
#define Font		XFont
#define GC		XGC
#define Point		XPoint
#define Rectangle	XRectangle
#define Screen		XScreen
#define Visual		XVisual
#define Window		XWindow

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>

#undef Colormap
#undef Cursor
#undef Display
#undef Drawable
#undef Font
#undef GC
#undef Point
#undef Rectangle
#undef Screen
#undef Visual
#undef Window

/*
 * Structure pointed to by X field of Memimage
 */
typedef struct Xmem Xmem;
struct Xmem
{
	int		pixmap;	/* pixmap id */
	XImage		*xi;	/* local image */
	int		dirty;	/* is the X server ahead of us?  */
	Rectangle	dirtyr;	/* which pixels? */
	Rectangle	r;	/* size of image */
};

typedef struct Xprivate Xprivate;
struct Xprivate {
	uint32		chan;
	XColormap	cmap;
	XCursor		cursor;
	XDisplay	*display;
	XDisplay	*snarfcon;
	XDisplay	*kmcon;
	int		fd;	/* of display */
	int		depth;				/* of screen */
	XDrawable	drawable;
	XColor		map[256];
	XColor		map7[128];
	uchar		map7to8[128][2];
	XGC		gccopy;
	XGC		gccopy0;
	XGC		gcfill;
	uint32		gcfillcolor;
	XGC		gcfill0;
	uint32		gcfill0color;
	XGC		gcreplsrc;
	uint32		gcreplsrctile;
	XGC		gcreplsrc0;
	uint32		gcreplsrc0tile;
	XGC		gcsimplesrc;
	uint32		gcsimplesrccolor;
	uint32		gcsimplesrcpixmap;
	XGC		gcsimplesrc0;
	uint32		gcsimplesrc0color;
	uint32		gcsimplesrc0pixmap;
	XGC		gczero;
	uint32		gczeropixmap;
	XGC		gczero0;
	uint32		gczero0pixmap;
	Rectangle	newscreenr;
	Memimage*	screenimage;
	QLock		screenlock;
	XDrawable	screenpm;
	XDrawable	nextscreenpm;
	Rectangle	screenr;
	int		toplan9[256];
	int		tox11[256];
	int		usetable;
	XVisual		*vis;
	Atom		clipboard;
	Atom		utf8string;
	Atom		targets;
	Atom		text;
	Atom		compoundtext;
	Atom		takefocus;
	Atom		losefocus;
	Atom		wmprotos;
	Atom		wmstate;
	Atom		wmfullscreen;
	uint		putsnarf;
	uint		assertsnarf;
	int		destroyed;
};

extern Xprivate _x;

extern Memimage *_xallocmemimage(Rectangle, uint32, int);
extern XImage	*_xallocxdata(Memimage*, Rectangle);
extern void	_xdirtyxdata(Memimage*, Rectangle);
extern void	_xfillcolor(Memimage*, Rectangle, uint32);
extern void	_xfreexdata(Memimage*);
extern XImage	*_xgetxdata(Memimage*, Rectangle);
extern void	_xputxdata(Memimage*, Rectangle);

long _xkeysym2rune(KeySym keysym);

struct Mouse;
extern int	_xtoplan9mouse(XEvent*, struct Mouse*);
extern void	_xtoplan9kbd(XEvent*);
extern void	_xexpose(XEvent*);
extern int	_xselect(XEvent*);
extern int	_xconfigure(XEvent*);
extern int	_xdestroy(XEvent*);
extern void	_flushmemscreen(Rectangle);
extern void	_xmoveto(Point);
struct Cursor;
extern void	_xsetcursor(struct Cursor*);
extern void	_xbouncemouse(Mouse*);
extern int		_xsetlabel(char*);
extern Memimage*	_xattach(char*, char*);
extern char*		_xgetsnarf(void);
extern void		_xputsnarf(char *data);
extern void		_xtopwindow(void);
extern void		_xresizewindow(Rectangle);
extern void		_xmovewindow(Rectangle);
extern int		_xreplacescreenimage(void);

#define MouseMask (\
	ButtonPressMask|\
	ButtonReleaseMask|\
	PointerMotionMask|\
	Button1MotionMask|\
	Button2MotionMask|\
	Button3MotionMask)

extern Rectangle screenrect;
extern Rectangle windowrect;
extern int fullscreen;

typedef struct Cursor Cursor;

enum
{
	PMundef = ~0
};
