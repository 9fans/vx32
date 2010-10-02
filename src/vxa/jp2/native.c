
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <vxa/vxa.h>
#include <vxa/codec.h>

#include "jasper/jasper.h"
#include "native.h"


#define BUFSIZE (64*1024)

char vxacodec_name[] = "jp2";


// Defined in djp2-bin.c, generated from djp2 by bin2c.pl
extern const uint8_t vxa_djp2_data[];
extern const int vxa_djp2_length;


static void init()
{
	static int inited;
	if (inited)
		return;
	inited = 1;

	int fmtid = 0;
	jas_image_fmtops_t fmtops;

	fmtops.decode = jp2_decode;
	fmtops.encode = NULL;
	fmtops.validate = jp2_validate;
	jas_image_addfmt(fmtid, "jp2", "jp2",
          "JPEG-2000 JP2 File Format Syntax (ISO/IEC 15444-1)", &fmtops);
	++fmtid;

	fmtops.decode = NULL;
	fmtops.encode = bmp_encode;
	fmtops.validate = NULL;
	jas_image_addfmt(fmtid, "bmp", "bmp",
		"Microsoft Bitmap (BMP)", &fmtops);
	++fmtid;
}

int vxacodec_init(struct vxacodec *c, struct vxaio *io)
{
	c->decoder = vxa_djp2_data;
	c->decodersize = vxa_djp2_length;
}

int vxacodec_encode(vxacodec *c, vxaio *io)
{
	return vxa_error(io, VXA_RC_WRONG_FORMAT,
			"JPEG 2000 codec does not yet support compression");
}

int vxacodec_decode(vxacodec *c, vxaio *io)
{
	init();

	if (io->method != VXA_M_JP2)
		return vxa_error(io, VXA_RC_WRONG_FORMAT,
				"jp2: unsupported method code %08x",
				io->method);

	jas_stream_t *in = jas_stream_vxaopen(io, JAS_STREAM_READ);
	if (in == NULL)
		return vxa_error(io, VXA_RC_NO_MEMORY, "out of memory");

	jas_stream_t *out = jas_stream_vxaopen(io, JAS_STREAM_WRITE);
	if (out == NULL)
		return vxa_error(io, VXA_RC_NO_MEMORY, "out of memory");

	jas_image_t *image = jas_image_decode(in, 0, NULL);
	if (image == NULL)
		return vxa_error(io, VXA_RC_UNKNOWN,
				"error decoding JP2 image");
			// XX can we be more specific?

	int rc = jas_image_encode(image, out, 1, NULL);
	if (rc != 0)
		return vxa_error(io, VXA_RC_UNKNOWN,
				"error writing BMP image");
			// XX can we be more specific?

	jas_stream_flush(out);

	jas_stream_close(in);
	jas_stream_close(out);

	jas_image_destroy(image);

	return VXA_RC_OK;
}

static uint8_t magic[] = {
	0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A,
	0x00, 0x00, 0x00, 0x14, 0x66, 0x74, 0x79, 0x70, 0x6A, 0x70, 0x32 };

int vxacodec_recognize(vxacodec *c, vxaio *io, const void *header, size_t size)
{
	assert(sizeof(magic) == 23);
	if (size >= 23 && memcmp(header, magic, 23) == 0) {
		io->method = VXA_M_JP2;
		return vxa_error(io, VXA_RC_COMPRESSED,
				"input already compressed in JP2 format");
	}

	// Format not recognized, but we could try compressing it anyway...
	return vxa_error(io, VXA_RC_WRONG_FORMAT,
			"JP2 codec currently can't compress");
}

