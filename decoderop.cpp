#include "decoderop.hpp"
#include "displayop.hpp"

#include <stdio.h>

void XKey::execute( DecoderState &state )
{
  switch ( key ) {
  case ' ':
    state.playing = !state.playing;
    break;
  case 'f':
    state.fullscreen = !state.fullscreen;
    {
      FullScreenMode *op = new FullScreenMode( state.fullscreen );
      state.oglq->leapfrog_enqueue( op, (DrawAndUnlockFrame*)NULL );
    }
    break;
  case 'q':
    state.live = false;
    break;
  case XK_Left:
    state.current_picture--;
    state.outputq.flush();
    state.outputq.enqueue( new MoveSlider( state.current_picture ) );
    break;
  case XK_Right:
    state.current_picture++;
    state.outputq.flush();
    state.outputq.enqueue( new MoveSlider( state.current_picture ) );
    break;
  default:
    fprintf( stderr, "key %d hit\n", (int)key );
    break;
  }
}

void DecoderShutDown::execute( DecoderState &state )
{
  state.live = false;
}
