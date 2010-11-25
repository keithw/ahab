#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "libmpeg2.h"

#include "file.hpp"
#include "es.hpp"

void progress_bar( off_t size, off_t location ) {}

int main( int argc, char *argv[] )
{
  struct timespec start, finish;

  unixassert( clock_gettime( CLOCK_REALTIME, &start ) );

  File *file = new File( argv[ 1 ] );
  ES *stream = new ES( file, &progress_bar );

  unixassert( clock_gettime( CLOCK_REALTIME, &finish ) );

  double secs = (finish.tv_sec - start.tv_sec)
    + (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

  int pic_count = stream->get_num_pictures();

  printf( "%d pictures in %.3f s = %.3f pics per second\n",
	  pic_count, secs, pic_count / secs );
}
