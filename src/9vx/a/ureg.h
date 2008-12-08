struct Ureg
{
	u32int	di;		/* general registers */
	u32int	si;		/* ... */
	u32int	bp;		/* ... */
	u32int	nsp;
	u32int	bx;		/* ... */
	u32int	dx;		/* ... */
	u32int	cx;		/* ... */
	u32int	ax;		/* ... */
	u32int	gs;		/* data segments */
	u32int	fs;		/* ... */
	u32int	es;		/* ... */
	u32int	ds;		/* ... */
	u32int	trap;		/* trap type */
	u32int	ecode;		/* error code (or zero) */
	u32int	pc;		/* pc */
	u32int	cs;		/* old context */
	u32int	flags;		/* old flags */
	union {
		u32int	usp;
		u32int	sp;
	};
	u32int	ss;		/* old stack segment */
};
