typedef struct Tos Tos;
typedef struct Plink Plink;

#if 0

struct Tos {
	struct			/* Per process profiling */
	{
		Plink	*pp;	/* known to be 0(ptr) */
		Plink	*next;	/* known to be 4(ptr) */
		Plink	*last;
		Plink	*first;
		ulong	pid;
		ulong	what;
	} prof;
	uvlong	cyclefreq;	/* cycle clock frequency if there is one, 0 otherwise */
	vlong	kcycles;	/* cycles spent in kernel */
	vlong	pcycles;	/* cycles spent in process (kernel + user) */
	ulong	pid;		/* might as well put the pid here */
	ulong	clock;
	/* top of stack is here */
};
#else

struct Tos {
	struct			/* Per process profiling */
	{
		uint32_t pp;	/* known to be 0(ptr) */
		uint32_t next;	/* known to be 4(ptr) */
		uint32_t last;
		uint32_t first;
		uint32_t	pid;
		uint32_t	what;
	} prof;
	uvlong	cyclefreq;	/* cycle clock frequency if there is one, 0 otherwise */
	vlong	kcycles;	/* cycles spent in kernel */
	vlong	pcycles;	/* cycles spent in process (kernel + user) */
	uint32_t	pid;		/* might as well put the pid here */
	uint32_t	clock;
	/* top of stack is here */
};
#endif

extern Tos *_tos;
