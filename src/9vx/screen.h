typedef struct Mouseinfo Mouseinfo;
typedef struct Mousestate Mousestate;
typedef struct Cursorinfo Cursorinfo;
typedef struct Screeninfo Screeninfo;

#define Mousequeue 16		/* queue can only have Mousequeue-1 elements */
#define Mousewindow 500		/* mouse event window in millisec */

struct Mousestate
{
	Point	xy;		/* mouse.xy */
	int	buttons;	/* mouse.buttons */
	ulong	counter;	/* increments every update */
	ulong	msec;		/* time of last event */
};

struct Mouseinfo
{
	Mousestate	mstate;
	int	dx;
	int	dy;
	int	track;		/* dx & dy updated */
	int	redraw;		/* update cursor on screen */
	ulong	lastcounter;	/* value when /dev/mouse read */
	ulong	lastresize;
	ulong	resize;
	Rendez	r;
	Ref	ref;
	QLock	qlk;
	int	open;
	int	inopen;
	int	acceleration;
	int	maxacc;
	Mousestate	queue[16];	/* circular buffer of click events */
	int	ri;		/* read index into queue */
	int	wi;		/* write index into queue */
	uchar	qfull;		/* queue is full */
};

struct Cursorinfo {
	Lock	lk;
	struct Cursor	cursor;
};

struct Screeninfo {
	Lock		lk;
	Memimage	*newsoft;
	int		reshaped;
	int		depth;
	int		dibtype;
};

extern	Mouseinfo mouse;
extern	Cursorinfo cursor;
extern	Screeninfo screen;
extern	Rectangle	mouserect;

enum
{
	SnarfSize = 1<<20
};

uchar*	attachscreen(Rectangle*, ulong*, int*, int*, int*, void**);
int	drawcanqlock(void);
void	drawqlock(void);
void	drawqunlock(void);
void	drawreplacescreenimage(Memimage*);
void	getcolor(ulong, ulong*, ulong*, ulong*);
char*	getsnarf(void);
void	flushmemscreen(Rectangle);
void	mousetrack(int, int, int, int);
void	putsnarf(char*);
int	setcolor(ulong, ulong, ulong, ulong);
void	setcursor(struct Cursor*);
void	setmouse(Point);
void	terminit(int);
void	termredraw(void);
void	termreplacescreenimage(Memimage*);
