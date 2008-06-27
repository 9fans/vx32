#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sha1.h"

int
main( int   argc,
      char *argv[] )
{
  unsigned char output[20];
  int i;

  if ( sha_stream( stdin, output ) != 0 ) {
    fprintf( stderr, "error reading stdin: errno=%i", errno );
    return 1;
  }

  for ( i = 0; i < sizeof( output ); i++ )
    printf("%02x", output[i]);

  return 0;
}
