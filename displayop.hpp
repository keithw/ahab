#ifndef DISPLAYOPERATION_HPP
#define DISPLAYOPERATION_HPP

class DisplayOperation;
class OpenGLDisplay;
class OpcodeState;

#include "framebuffer.hpp"
#include "ogl.hpp"

class DisplayOperation {
public:
  virtual void execute( OpcodeState &state ) = 0;
  virtual ~DisplayOperation() {}
};

class Repaint : public DisplayOperation {
public:
  Repaint() {}
  ~Repaint() {}
  void execute( OpcodeState &state );
};

class DrawAndUnlockFrame : public DisplayOperation {
private:
  FrameHandle *handle;

  static void load_tex( GLenum tnum, GLuint tex, uint width, uint height,
			uint8_t *data );

public:
  DrawAndUnlockFrame( FrameHandle *s_handle ) { handle = s_handle; }
  ~DrawAndUnlockFrame() { handle->decrement_lockcount(); }
  void execute( OpcodeState &state );
};

class LoadMatrixCoefficients : public DisplayOperation {
private:
  double green[ 3 ], blue[ 3 ], red[ 3 ];

public:
  LoadMatrixCoefficients( double s_green[ 3 ],
			  double s_blue[ 3 ],
			  double s_red[ 3 ] ) {
    for ( int i = 0; i < 3; i++ ) {
      green[ i ] = s_green[ i ];
      blue[ i ] = s_blue[ i ];
      red[ i ] = s_red[ i ];
    }
  }

  ~LoadMatrixCoefficients() {}
  void execute( OpcodeState &state );
};

class ShutDown : public DisplayOperation {
public:
  ShutDown() {}
  ~ShutDown() {}
  void execute( OpcodeState &state );
};

class FullScreenMode : public DisplayOperation {
private:
  bool fullscreen;

public:
  FullScreenMode( bool s_fullscreen ) { fullscreen = s_fullscreen; }
  ~FullScreenMode() {}
  void execute( OpcodeState &state );
};

class NullOperation : public DisplayOperation {
public:
  NullOperation( void ) {}
  ~NullOperation( void ) {}
  void execute( OpcodeState &state ) {}
};

#endif
