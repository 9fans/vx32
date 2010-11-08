
#include <stdlib.h>
#include <unistd.h>


static void noop(void) { }

void (*__exit_flush)(void) = noop;	// flush stdio streams on exit
void (*__exit_atexit)(void) = noop;	// call atexit handlers on exit

void exit(int status)
{
	// Call all atexit handlers
	__exit_atexit();

	// Flush stdio streams
	__exit_flush();

	// Hard-exit to the parent
	_exit(status);
}

