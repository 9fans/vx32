
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char **argv)
{
	assert(argc >= 3);	// yeah, really user-friendly...

	int reps = atoi(argv[1]);
	assert(reps >= 1);

	for (int i = 0; i < reps; i++) {
		pid_t pid = vfork();
		if (pid == 0) {	// in the child
			execv(argv[2], &argv[2]);
			perror("exec");
			abort();
		}
		if (pid < 0) {
			perror("vfork");
			abort();
		}
		waitpid(pid, NULL, 0);
	}
	return 0;
}

