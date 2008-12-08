/*
 * Memory and machine-specific definitions.  Used in C and assembler.
 */

/*
 * Sizes
 */
#define	BI2BY		8			/* bits per byte */
#define	BI2WD		32			/* bits per word */
#define	BY2WD		4			/* bytes per word */
#define	BY2V		8			/* bytes per double word */
#define	BY2PG		4096			/* bytes per page */
#define	WD2PG		(BY2PG/BY2WD)		/* words per page */
#define	BY2XPG		(4096*1024)	/* bytes per big page */
#define	PGSHIFT		12			/* log(BY2PG) */
#define	ROUND(s, sz)	(((s)+((sz)-1))&~((sz)-1))
#define	PGROUND(s)	ROUND(s, BY2PG)
#define	BLOCKALIGN	8

#define	MAXMACH		128			/* max # cpus system can run */
#define	KSTACK		(1024*1024)			/* Size of kernel stack */

/*
 * Time
 */
#define	HZ		(1000)			/* clock frequency */
#define	MS2HZ		(1000/HZ)		/* millisec per clock tick */
#define	TK2SEC(t)	((t)/HZ)		/* ticks to seconds */

/*
 *  Address spaces - only user code!
 */
#define	UZERO		0			/* base of user address space */
#define	UTZERO		(UZERO+BY2PG)		/* first address in user text */
#define	USTKTOP		(0x10000000)		/* byte just beyond user stack */
#define	USTKSIZE	(16*1024*1024)		/* size of user stack */
#define	TSTKTOP		(USTKTOP-USTKSIZE)	/* end of new stack in sysexec */
#define	TSTKSIZ 	100

/*
 *  virtual MMU
 */
#define	PTEMAPMEM	(1024*1024)	
#define	PTEPERTAB	(PTEMAPMEM/BY2PG)
#define	SEGMAPSIZE	1984
#define	SSEGMAPSIZE	16
#define	PPN(x)		((x)&~(BY2PG-1))

/*
 *  physical MMU
 */
#define	PTEVALID	(1<<0)
#define	PTEWT		(1<<3)
#define	PTEUNCACHED	(1<<4)
#define	PTEWRITE	(1<<1)
#define	PTERONLY	(0<<1)
#define	PTEKERNEL	(0<<2)
#define	PTEUSER		(1<<2)
#define	PTESIZE		(1<<7)
#define	PTEGLOBAL	(1<<8)

#define	getpgcolor(a)	0

