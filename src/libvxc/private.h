#ifndef CLIB_PRIVATE_H
#define CLIB_PRIVATE_H


// exit.c
extern void (*__exit_flush)(void);	// flush stdio streams on exit
extern void (*__exit_atexit)(void);	// call atexit handlers on exit


#endif	// CLIB_PRIVATE_H
