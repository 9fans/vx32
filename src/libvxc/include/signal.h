#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <sys/types.h>


/* Signal-related types */
typedef void (*sighandler_t)(int);
typedef unsigned long	sigset_t;
struct sigaction;

/* Signal numbers */
#define SIGHUP		1
#define SIGINT		2
#define SIGQUIT		3
#define SIGILL		4
#define SIGTRAP		5
#define SIGABRT		6
#define SIGBUS		7
#define SIGFPE		8
#define SIGKILL		9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGPOLL		29
#define SIGSYS		31


/* Special signal function values */
#define SIG_ERR		((sighandler_t)-1)
#define SIG_DFL		((sighandler_t)0)
#define SIG_IGN		((sighandler_t)1)
#define SIG_HOLD	((sighandler_t)2)


/* Signal handling */
struct sigaction {
	union {
		void (*sa_handler)(int);
		void (*sa_sigaction)(int siginfo_t, void*);
	};
	sigset_t sa_mask;
	int sa_flags;
};

#define SA_NOCLDSTOP	0x0001
#define SA_NOCLDWAIT	0x0002
#define SA_NODEFER	0x0004
#define SA_SIGINFO	0x0008
#define SA_ONSTACK	0x0010
#define SA_RESETHAND	0x0020
#define SA_RESTART	0x0040


typedef volatile int sig_atomic_t;


/* Signal sets */
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int sig);
int sigdelset(sigset_t *set, int sig);
int sigismember(const sigset_t *set, int sig);

/* Signal generation */
int kill(pid_t target, int sig);
int raise(int sig);

/* Signal management */
sighandler_t signal(int sig, sighandler_t handler);
int sigaction(int sig, const struct sigaction *__restrict act,
		struct sigaction *__restrict oldact);
int sigprocmask(int how, const sigset_t *__restrict set,
		sigset_t *__restrict oldset);
int sigpending(sigset_t *set);
int sigsuspend(const sigset_t *mask);
int sigwait(const sigset_t *__restrict set, int *__restrict sig);


#endif	// _SIGNAL_H
