#include "decoderop.hpp"
#include "displayop.hpp"

#include <stdio.h>

void XKey::execute( DecoderState &state )
{
  switch ( key ) {
  case '@':
    {
      Repaint *op = new Repaint();
      state.oglq->enqueue( op );
    }
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
  }
}

void DecoderShutDown::execute( DecoderState &state )
{
  state.live = false;
}
