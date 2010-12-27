#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "displayop.hpp"

void DrawAndUnlockFrame::execute( OpcodeState &state )
{
  state.draw( handle->get_frame()->get_buf() );
}

void LoadMatrixCoefficients::execute( OpcodeState &state )
{
  state.load_matrix_coefficients( green, blue, red );
}

void ShutDown::execute( OpcodeState & )
{
  pthread_exit( NULL );
}

void FullScreenMode::execute( OpcodeState &state )
{
  if ( fullscreen ) {
    state.dofullscreen();
  } else {
    state.unfullscreen();
  }
}

void Repaint::execute( OpcodeState &state )
{
  state.paint();
}
