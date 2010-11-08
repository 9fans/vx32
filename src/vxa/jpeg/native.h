
#include <setjmp.h>
#include <vxa/vxa.h>

#define BUFSIZE (64*1024)

struct client_data {
	vxaio *io;
	jmp_buf errjmp;
	uint8_t *inbuf;
	uint8_t *outbuf;
	unsigned outpos;
};

void vxajpeg_flush(struct client_data *cd);

