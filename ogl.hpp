#ifndef OGL_HPP
#define OGL_HPP

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <stdint.h>

#include "displayopq.hpp"
#include "displayop.hpp"

class OpcodeState {
private:
  static void load_tex( GLenum tnum, GLuint tex,
			uint width, uint height, uint8_t *data );

public:
  Display *display;
  Window window;
  GLXContext context;
  uint width, height; /* window size on screen */
  uint framewidth, frameheight; /* luma matrix dimensions */
  uint dispwidth, dispheight; /* MPEG-2 intended display size */
  double sar;

  void draw( uint8_t *ycbcr );
  void paint( void );
  void window_setup( void );
  void reset_viewport( void );

  void dofullscreen( void );
  void unfullscreen( void );

  void load_matrix_coefficients( double green[ 3 ],
				 double blue[ 3 ],
				 double red[ 3 ] );
  GLuint Y_tex, Cb_tex, Cr_tex;
};

class OpenGLDisplay {
 private:
  OpcodeState state;

  GLuint shader;

  void init_context( void );
  static void init_tex( GLenum tnum, GLint internalformat, GLuint *tex,
			uint width, uint height, GLint interp );


  pthread_t thread_handle;
  Queue<DisplayOperation> opq;

 public:
  OpenGLDisplay( char *display_name, double movie_sar,
		 uint s_framewidth, uint s_frameheight,
		 uint s_dispwidth, uint s_dispheight );
  ~OpenGLDisplay();
  bool getevent( bool block, XEvent *ev );
  void makeevent( void );

  void loop( void );

  Queue<DisplayOperation> *get_queue() { return &opq; }

  static void GLcheck( const char *where ) {
    GLenum GLerror;
    
    if ( (GLerror = glGetError()) != GL_NO_ERROR ) {
      fprintf( stderr, "GL error (%x) at (%s) (%s).\n", GLerror, where,
	       gluErrorString( GLerror ) );
    }
  }
};

#endif
