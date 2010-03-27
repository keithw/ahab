#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "libmpeg2.h"

#include "file.hpp"
#include "es.hpp"
#include "mpegheader.hpp"
#include "exceptions.hpp"
#include "framebuffer.hpp"

void progress_bar( off_t size, off_t location ) {}

int main( int argc, char *argv[] )
{
  if ( argc != 3 ) {
    fprintf( stderr, "USAGE: %s FILENAME PARALLEL\n", argv[ 0 ] );
    exit( 1 );
  }

  bool parallel = atoi( argv[ 2 ] );

  File *file = new File( argv[ 1 ] );
  ES *stream = new ES( file, &progress_bar );
  int num_pictures = stream->get_num_pictures();

  struct timespec start, finish;

  unixassert( clock_gettime( CLOCK_REALTIME, &start ) );

  int pic_count = 0;

  for ( int i = 0; i < num_pictures; i++ ) {
    if ( parallel ) {
      stream->get_picture_displayed( i )->start_parallel_decode( true );
      stream->get_picture_displayed( i )->get_framehandle()->wait_rendered();
    } else {
      stream->get_picture_displayed( i )->lock_and_decodeall();
    }
    
    stream->get_picture_displayed( i )->get_framehandle()->decrement_lockcount();
    pic_count++;
  }

  unixassert( clock_gettime( CLOCK_REALTIME, &finish ) );

  double secs = (finish.tv_sec - start.tv_sec)
    + (finish.tv_nsec - start.tv_nsec) / 1000000000.0;

  printf( "%d pictures in %.3f s = %.3f pics per second\n",
	  pic_count, secs, pic_count / secs );
}
