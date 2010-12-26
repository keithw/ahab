#include "ogl.hpp"

#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "ahab_fragment_program.hpp"
#include "exceptions.hpp"
#include "colorimetry.hpp"

#include "displayopq.hpp"

const int opqueue_len = 8;

static void *thread_helper( void *ogl )
{
  OpenGLDisplay *me = static_cast<OpenGLDisplay *>( ogl );
  ahabassert( me );
  me->loop();
  return NULL;
}

OpenGLDisplay::OpenGLDisplay( char *display_name,
			      double movie_sar,
			      uint s_framewidth, uint s_frameheight,
			      uint s_dispwidth, uint s_dispheight )
  : opq( opqueue_len )
{
  state.framewidth = s_framewidth;
  state.frameheight = s_frameheight;
  state.dispwidth = s_dispwidth;
  state.dispheight = s_dispheight;
  
  if ( 0 == XInitThreads() ) {
    fprintf( stderr, "XInitThreads() failed." );
    throw DisplayError();
  }

  state.display = XOpenDisplay( display_name );
  if ( state.display == NULL ) {
    fprintf( stderr, "Couldn't open display.\n" );
    throw DisplayError();
  }

  /* Figure out the dimensions given the width and height,
     the video sample aspect ratio, and the screen pixel aspect ratio */
  double display_sar = ((double)DisplayHeight( state.display, DefaultScreen( state.display ) )
			/ (double)DisplayHeightMM( state.display, DefaultScreen( state.display ) ))
    / ((double)DisplayWidth( state.display, DefaultScreen( state.display ) )
       / (double)DisplayWidthMM( state.display, DefaultScreen( state.display ) ));

  state.sar = movie_sar / display_sar;

  // sar = 1; /* XXX */

  if ( state.sar > 1 ) {
    state.width = lrint( (double)state.dispwidth * state.sar );
    state.height = state.dispheight;
  } else {
    state.width = state.dispwidth;
    state.height = lrint( (double)state.dispheight / state.sar );
  }

  fprintf( stderr, "Display is %dx%d with pixel AR %.3f:1, display AR %.3f:1.\n",
	   DisplayWidth( state.display, DefaultScreen( state.display ) ),
	   DisplayHeight( state.display, DefaultScreen( state.display ) ),
	   display_sar,
	   (double)DisplayWidthMM( state.display, DefaultScreen( state.display ) )
	   / (double)DisplayHeightMM( state.display, DefaultScreen( state.display ) ) );

  fprintf( stderr, "MPEG-2 sequence is %dx%d with sample AR %.3f:1, display AR %.3f:1.\n",
	   state.dispwidth, state.dispheight,
	   movie_sar,
	   movie_sar * state.dispwidth / (double)state.dispheight );

  fprintf( stderr, "Video SAR in display pixel units = %.3f:1. Display size = %dx%d.\n",
	   state.sar, state.width, state.height );

  state.GetSync = (Bool (*)(Display*, GLXDrawable, int64_t*, int64_t*, int64_t*))glXGetProcAddress( (GLubyte *) "glXGetSyncValuesOML" );

  ahabassert( state.GetSync );

  state.last_mbc = -1;
  state.last_us = -1;

  unixassert( pthread_create( &thread_handle, NULL,
			      thread_helper, this ) );

  int prio = sched_get_priority_max( SCHED_FIFO );
  ahabassert( prio != -1 );

  struct sched_param params;
  params.sched_priority = prio;

  unixassert( pthread_setschedparam( thread_handle,
				     SCHED_FIFO, &params ) );
}

void OpenGLDisplay::init_context( void ) {
  int attributes[] = { GLX_RGBA,
		       GLX_DOUBLEBUFFER, True,
		       GLX_RED_SIZE, 8,
		       GLX_GREEN_SIZE, 8,
		       GLX_BLUE_SIZE, 8,
		       None };

  XVisualInfo *visual = glXChooseVisual( state.display, 0, attributes );
  if ( visual == NULL ) {
    fprintf( stderr, "Could not open glX visual.\n" );
    throw DisplayError();
  }

  state.context = glXCreateContext( state.display, visual, NULL, True );
  if ( state.context == NULL ) {
    fprintf( stderr, "No glX context.\n" );
    throw DisplayError();
  }

  XFree( visual );

  if ( !glXMakeCurrent( state.display, state.window, state.context ) ) {
    fprintf( stderr, "Could not reactivate OpenGL.\n" );
    throw DisplayError();
  }

  GLcheck( "glXMakeCurrent" );

  /* initialize textures */
  init_tex( GL_TEXTURE0, GL_LUMINANCE8, &state.Y_tex,
	    state.framewidth, state.frameheight, GL_LINEAR );
  init_tex( GL_TEXTURE1, GL_LUMINANCE8, &state.Cb_tex,
	    state.framewidth/2, state.frameheight/2, GL_LINEAR );
  init_tex( GL_TEXTURE2, GL_LUMINANCE8, &state.Cr_tex,
	    state.framewidth/2, state.frameheight/2, GL_LINEAR );

  /* load the shader */
  GLint errorloc;  
  glEnable( GL_FRAGMENT_PROGRAM_ARB );
  glGenProgramsARB( 1, &shader );
  glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, shader );
  glProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB,
		      strlen( ahab_fragment_program ),
		      ahab_fragment_program );
  glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorloc );
  if ( errorloc != -1 ) {
    fprintf( stderr, "Error in fragment shader at position %d.\n", errorloc );
    fprintf( stderr, "Error string: %s\n",
	     glGetString( GL_PROGRAM_ERROR_STRING_ARB ) );
    throw DisplayError();
  }

  GLcheck( "glProgramString" );

  /* guess colors */
  if ( state.frameheight <= 480 ) {
    smpte170m.execute( state );
  } else {
    itu709.execute( state );
  }
}

void OpcodeState::reset_viewport( void )
{
  glXSwapBuffers( display, window );
  glFinish();
  OpenGLDisplay::GLcheck( "reset_viewport: glFinish" );
  XSync( display, False );
  glLoadIdentity();
  glViewport( 0, 0, width, height );
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  glOrtho( 0, width, height, 0, -1, 1 );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
  glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
  OpenGLDisplay::GLcheck( "glPixelStorei" );
  glWindowPos2d( 0, 0 );
  glClear( GL_COLOR_BUFFER_BIT );
  OpenGLDisplay::GLcheck( "reset_viewport" );
}

void OpcodeState::window_setup( void )
{
  window = XCreateSimpleWindow( display, DefaultRootWindow( display ),
				0, 0, width, height, 0, 0, 0 );
  XSetStandardProperties( display, window, "Ahab", "Ahab",
			  None, NULL, 0, NULL );
  XSelectInput( display, window, ExposureMask | KeyPressMask );
}

OpenGLDisplay::~OpenGLDisplay()
{
  opq.flush();

  DisplayOperation *shutdown = new ShutDown();
  opq.enqueue( shutdown );

  unixassert( pthread_join( thread_handle, NULL ) );

  delete shutdown; /* Doesn't get deleted by loop because opcode execute() exits first. */

  glDeleteProgramsARB( 1, &shader );
  glDeleteTextures( 1, &state.Y_tex );
  glDeleteTextures( 1, &state.Cb_tex );
  glDeleteTextures( 1, &state.Cr_tex );
  glXDestroyContext( state.display, state.context );
  XDestroyWindow( state.display, state.window );
  XCloseDisplay( state.display );
}

void OpenGLDisplay::init_tex( GLenum tnum, GLint internalformat, GLuint *tex,
			      uint width, uint height, GLint interp )
{
  glActiveTexture( tnum );
  glEnable( GL_TEXTURE_RECTANGLE_ARB );
  glGenTextures( 1, tex );
  glBindTexture( GL_TEXTURE_RECTANGLE_ARB, *tex );
  glTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
		internalformat, width, height,
		0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, interp );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, interp );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
  glTexParameteri( GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
  glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
  GLcheck( "init_tex" );
}

void OpcodeState::paint( void )
{
  glPushMatrix();
  glLoadIdentity();
  glTranslatef( 0, 0, 0 );
  glBegin( GL_POLYGON );

  const double ff = 1.0/128; /* Mesa fudge factor */
  const double xoffset = 0.25; /* MPEG-2 style 4:2:0 subsampling */

  glMultiTexCoord2d( GL_TEXTURE0, ff, ff );
  glMultiTexCoord2d( GL_TEXTURE1, xoffset+ff, ff );
  glMultiTexCoord2d( GL_TEXTURE2, xoffset+ff, ff );
  glVertex2s( 0, 0 );

  glMultiTexCoord2d( GL_TEXTURE0, dispwidth+ff, ff );
  glMultiTexCoord2d( GL_TEXTURE1, dispwidth/2 + xoffset + ff, ff );
  glMultiTexCoord2d( GL_TEXTURE2, dispwidth/2 + xoffset + ff, ff );
  glVertex2s( width, 0 );

  glMultiTexCoord2d( GL_TEXTURE0, dispwidth+ff, dispheight+ff );
  glMultiTexCoord2d( GL_TEXTURE1, dispwidth/2 + xoffset + ff, dispheight/2 + ff);
  glMultiTexCoord2d( GL_TEXTURE2, dispwidth/2 + xoffset + ff, dispheight/2 + ff);
  glVertex2s( width, height);

  glMultiTexCoord2d( GL_TEXTURE0, ff, dispheight+ff );
  glMultiTexCoord2d( GL_TEXTURE1, xoffset+ff, dispheight/2 + ff );
  glMultiTexCoord2d( GL_TEXTURE2, xoffset+ff, dispheight/2 + ff );
  glVertex2s( 0, height);

  glEnd();

  glPopMatrix();

  glXSwapBuffers( display, window );

  OpenGLDisplay::GLcheck( "glXSwapBuffers" );

  glFinish();

  /* Check if we stuttered */  

  int64_t ust, mbc, sbc, us;
  struct timeval now;

  gettimeofday( &now, NULL );
  us = 1000000 * now.tv_sec + now.tv_usec;
  long us_diff = us - last_us;

  GetSync( display, window, &ust, &mbc, &sbc );

  if ( (last_mbc != -1) && (mbc != last_mbc + 1) ) {
    long int diff = mbc - last_mbc;
    fprintf( stderr, "Skipped %ld retraces.\n", diff );
  }

  if ( (last_us != -1) && ( (us_diff < 8000) || (us_diff > 24000) ) ) {
    fprintf( stderr, "Time diff was %ld us.\n", (long)us_diff );
  }

  last_mbc = mbc;
  last_us = us;
}

typedef struct
{
    long flags;
    long functions;
    long decorations;
    long input_mode;
    long state;
} MotifWmHints;

#define MWM_HINTS_DECORATIONS   (1L << 1)

void OpcodeState::dofullscreen( void )
{
  /* move on top */
  XEvent xev;
  Atom wm_state = XInternAtom( display, "_NET_WM_STATE", False);
  Atom fullscreen = XInternAtom( display, "_NET_WM_STATE_FULLSCREEN", False);

  memset(&xev, 0, sizeof(xev));
  xev.type = ClientMessage;
  xev.xclient.window = window;
  xev.xclient.message_type = wm_state;
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 1;
  xev.xclient.data.l[1] = fullscreen;
  xev.xclient.data.l[2] = 0;

  XSendEvent( display, DefaultRootWindow( display ), False,
	      SubstructureNotifyMask, &xev);

  /* hide cursor */
  Cursor thecursor;
  Pixmap thepixmap;
  char no_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  XColor bg;

  thepixmap = XCreateBitmapFromData( display, window, no_data, 8, 8 );
  thecursor = XCreatePixmapCursor( display, thepixmap, thepixmap, &bg, &bg, 0, 0 );
  XDefineCursor( display, window, thecursor );
  XFreeCursor( display, thecursor );
  XFreePixmap( display, thepixmap );

  /* adjust width and height of displayed image */
  width = DisplayWidth( display, DefaultScreen( display ) );
  height = lrint( (double)dispheight * (double)width / ((double)dispwidth * sar) );

  if ( (signed)height > DisplayHeight( display, DefaultScreen( display ) ) ) {
    height = DisplayHeight( display, DefaultScreen( display ) );
    width = lrint( (double)dispwidth * (double) sar * (double)height / (double)dispheight );
  }

  reset_viewport();

  OpenGLDisplay::GLcheck( "fullscreen" );

  paint();
}

void OpcodeState::unfullscreen( void )
{
  XDestroyWindow( display, window );

  /* adjust width and height of displayed image */
  if ( sar > 1 ) {
    width = lrint( (double)dispwidth * sar );
    height = dispheight;
  } else {
    width = dispwidth;
    height = lrint( (double)dispheight / sar );
  }

  window_setup();

  if ( !glXMakeCurrent( display, window, context ) ) {
    fprintf( stderr, "Could not reactivate OpenGL.\n" );
    throw DisplayError();
  }
  
  OpenGLDisplay::GLcheck( "glXMakeCurrent" );

  reset_viewport();

  XMapRaised( display, window );

  paint();
}

void OpenGLDisplay::makeevent( void )
{
  XEvent event;
  memset( &event, 0, sizeof( event ) );
  event.type = Expose;
  if ( 0 == XSendEvent( state.display, state.window, False, ExposureMask, &event ) ) {
    throw DisplayError();
  };
  XFlush( state.display );
}

bool OpenGLDisplay::getevent( bool block, XEvent *ev )
{
  if ( block || XPending( state.display ) ) {
    XNextEvent( state.display, ev );
    return true;
  } else {
    return false;
  }
}

void OpcodeState::load_matrix_coefficients( double green[ 3 ],
					    double blue[ 3 ],
					    double red[ 3 ] )
{
  glProgramLocalParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, 0,
				green[ 0 ], green[ 1 ], green[ 2 ], 0 );
  glProgramLocalParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, 1,
				blue[ 0 ], blue[ 1 ], blue[ 2 ], 0 );
  glProgramLocalParameter4dARB( GL_FRAGMENT_PROGRAM_ARB, 2,
				red[ 0 ], red[ 1 ], red[ 2 ], 0 );
  OpenGLDisplay::GLcheck( "glProgramEnvParamater4dARB" );
}

void OpcodeState::load_tex( GLenum tnum, GLuint tex,
			     uint width, uint height, uint8_t *data )
{
  glActiveTexture( tnum );
  OpenGLDisplay::GLcheck( "glActiveTexture" );
  glBindTexture( GL_TEXTURE_RECTANGLE_ARB, tex );
  OpenGLDisplay::GLcheck( "glBindTexture" );
  glTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, width, height,
		   GL_LUMINANCE, GL_UNSIGNED_BYTE, data );
  OpenGLDisplay::GLcheck( "glTexSubImage2D" );
}

void OpcodeState::draw( uint8_t *ycbcr )
{
  load_tex( GL_TEXTURE0, Y_tex, framewidth, frameheight, ycbcr );
  load_tex( GL_TEXTURE1, Cb_tex, framewidth/2, frameheight/2,
	    ycbcr + framewidth * frameheight );
  load_tex( GL_TEXTURE2, Cr_tex, framewidth/2, frameheight/2,
	    ycbcr + framewidth * frameheight + framewidth * frameheight / 4);
  
  paint();
}

void OpenGLDisplay::loop( void )
{
  state.window_setup();
  XMapRaised( state.display, state.window );

  init_context();
  state.reset_viewport();

  while ( 1 ) {
    DisplayOperation *op = opq.dequeue( true );
    op->execute( state );
    delete op;
  }
}
