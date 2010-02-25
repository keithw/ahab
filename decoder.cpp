#include <pthread.h>

#include "decoder.hpp"
#include "exceptions.hpp"

#include "mpegheader.hpp"
#include "displayop.hpp"
#include "decoderop.hpp"

static void *thread_helper( void *decoder )
{
  Decoder *me = static_cast<Decoder *>( decoder );
  ahabassert( me );
  me->loop();
  return NULL;
}

Decoder::Decoder( ES *s_stream,
		  OperationQueue<DisplayOperation> *s_oglq )
  : opq( 0 ),
    stream( s_stream )
{
  state.current_picture = 0;
  state.fullscreen = false;
  state.live = true;
  state.oglq = s_oglq;

  pthread_create( &thread_handle, NULL, thread_helper, this );
}

Decoder::~Decoder() {}

void Decoder::decode_and_display( void )
{
  Picture *pic = stream->get_picture_displayed( state.current_picture );
  pic->lock_and_decodeall();
  DrawAndUnlockFrame *op = new DrawAndUnlockFrame( pic->get_framehandle() );
  state.oglq->flush_type( op );
  state.oglq->enqueue( op );  
}

void Decoder::loop( void )
{
  decode_and_display();

  int picture_displayed = state.current_picture;

  while ( state.live ) {
    if ( state.current_picture < 0 ) {
      state.current_picture = 0;
    } else if ( (uint)state.current_picture >= stream->get_num_pictures() ) {
      state.current_picture = stream->get_num_pictures() - 1;
    }

    if ( state.current_picture != picture_displayed ) {
      decode_and_display();
    }

    picture_displayed = state.current_picture;

    DecoderOperation *op = opq.dequeue( true );
    op->execute( state );
    delete op;
  }
}

void Decoder::wait_shutdown( void )
{
  unixassert( pthread_join( thread_handle, NULL ) );
}
