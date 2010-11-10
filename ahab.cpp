#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libmpeg2.h"

#include "controller.hpp"
#include "file.hpp"
#include "es.hpp"
#include "ogl.hpp"
#include "decoder.hpp"
#include "xeventloop.hpp"

#include <sys/time.h>
#include <math.h>

void progress_bar( off_t size, off_t location );

int main( int argc, char *argv[] )
{
  File *file;
  ES *stream;
  Sequence *seq;
  OpenGLDisplay *display;
  Controller *controller;
  Decoder *decoder;
  XEventLoop *xevents;

  if ( argc != 2 ) {
    fprintf( stderr, "USAGE: %s FILENAME\n", argv[ 0 ] );
    exit( 1 );
  }

  fprintf( stderr, "Opening file..." );
  file = new File( argv[ 1 ] );
  fprintf( stderr, " done.\n" );

  fprintf( stderr, "Constructing elementary stream object...      " );
  try {
    stream = new ES( file, &progress_bar );
  } catch ( AhabException *e ) {
    fprintf( stderr, "Caught exception.\n" );
    if ( UnixError *ue = dynamic_cast<UnixError *>( e ) ) {
      fprintf( stderr, "UnixError( %d )\n", ue->err );
    }
    return 1;
  }

  fprintf( stderr, "\b\b\b\b\b\b\b done. \n" );

  seq = stream->get_sequence();

  display = new OpenGLDisplay( (char *)NULL, seq->get_sar(),
			       16 * seq->get_mb_width(),
			       16 * seq->get_mb_height(),
			       seq->get_horizontal_size(),
			       seq->get_vertical_size() );

  fprintf( stderr, "Pictures: %d, duration: %.3f seconds.\n",
	   stream->get_num_pictures(), stream->get_duration() );

  controller = new Controller( stream->get_num_pictures() );

  decoder = new Decoder( stream, display->get_queue() );

  xevents = new XEventLoop( display );

  controller->get_queue()->hookup( decoder->get_queue() );
  xevents->get_key_queue()->hookup( decoder->get_queue() );
  xevents->get_repaint_queue()->hookup( display->get_queue() );

  decoder->get_output_queue()->hookup( controller->get_input_queue() );

  try {
    decoder->wait_shutdown();
  } catch ( UnixAssertError *e ) {
    e->print();
  }

  try {
    delete xevents;
    delete controller;
    delete decoder;
    delete display;
    delete stream;
    delete file;
  } catch ( UnixAssertError *e ) {
    e->print();
  }

  return 0;
}

void progress_bar( off_t size, off_t location )
{
  static char percent[ 20 ] = "";
  char new_percent[ 20 ] = "";
  char backspaces[ 6 ] = "\b\b\b\b\b";

  snprintf( new_percent, 20, "%2.0f", 100.0 * location / (double)size );

  if ( (percent[ 1 ] != new_percent[ 1 ])
       || (percent[ 0 ] != new_percent[ 0 ]) ) {
    fprintf( stderr, "%s[%s%%]", backspaces, new_percent );    
  }

  percent[ 0 ] = new_percent[ 0 ];
  percent[ 1 ] = new_percent[ 1 ];
}
