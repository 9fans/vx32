#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "sha2.h"


#define BLOCKSIZE 4096
/* Ensure that BLOCKSIZE is a multiple of 64.  */
#if BLOCKSIZE % 64 != 0
/* FIXME-someday (soon?): use #error instead of this kludge.  */
"invalid BLOCKSIZE"
#endif


/* Compute SHA512 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
int
sha512_stream (FILE *stream, void *resblock)
{
  SHA512_CTX ctx;
  char buffer[BLOCKSIZE + 72];
  size_t sum;

  /* Initialize the computation context.  */
  SHA512_Init (&ctx);

  /* Iterate over full file contents.  */
  while (1)
    {
      /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
	 computation function processes the whole buffer so that with the
	 next round of the loop another block can be read.  */
      size_t n;
      sum = 0;

      /* Read block.  Take care for partial reads.  */
      while (1)
	{
	  n = fread (buffer + sum, 1, BLOCKSIZE - sum, stream);

	  sum += n;

	  if (sum == BLOCKSIZE)
	    break;

	  if (n == 0)
	    {
	      /* Check for the error flag IFF N == 0, so that we don't
		 exit the loop after a partial read due to e.g., EAGAIN
		 or EWOULDBLOCK.  */
	      if (ferror (stream))
		return 1;
	      goto process_partial_block;
	    }

	  /* We've read at least one byte, so ignore errors.  But always
	     check for EOF, since feof may be true even though N > 0.
	     Otherwise, we could end up calling fread after EOF.  */
	  if (feof (stream))
	    goto process_partial_block;
	}

      /* Process buffer with BLOCKSIZE bytes.  Note that
			BLOCKSIZE % 64 == 0
       */
      SHA512_Update (&ctx, buffer, BLOCKSIZE);
    }

 process_partial_block:;

  /* Process any remaining bytes.  */
  if (sum > 0)
    SHA512_Update (&ctx, buffer, sum);

  /* Construct result in desired memory.  */
  SHA512_Final (resblock, &ctx);
  return 0;
}

int
main( int   argc,
      char *argv[] )
{
  unsigned char output[SHA512_DIGEST_LENGTH];
  int i;

  if ( sha512_stream( stdin, output ) != 0 ) {
    fprintf( stderr, "error reading stdin: errno=%i", errno );
    return 1;
  }

  for ( i = 0; i < sizeof( output ); i++ )
    printf("%02x", output[i]);

  return 0;
}

