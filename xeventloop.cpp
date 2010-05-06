#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <X11/Xlib.h>

#include "exceptions.hpp"
#include "xeventloop.hpp"

static void *thread_helper( void *xeventloop )
{
  XEventLoop *me = static_cast<XEventLoop *>( xeventloop );
  ahabassert( me );
  me->loop();
  return NULL;
}

XEventLoop::XEventLoop( OpenGLDisplay *s_display )
  : keys( 0 ),
    repaints( 0 ),
    display( s_display ),
    live( true )
{
  unixassert( pthread_mutex_init( &mutex, NULL ) );
  pthread_create( &thread_handle, NULL, thread_helper, this );  
}

void XEventLoop::loop( void )
{
  XEvent ev;
  XExposeEvent        *expose = (XExposeEvent *)&ev;
  XKeyEvent           *key    = (XKeyEvent *)&ev;

  while ( 1 ) {
    display->getevent( true, &ev );

    { MutexLock x( &mutex ); if ( !live ) return; }

    if ( ev.type == Expose && expose->count == 0 ) {
      repaints.enqueue( new Repaint() );
    } else if ( ev.type == KeyPress ) {
      KeySym keysym = XLookupKeysym( key, 0 );
      keys.enqueue( new XKey( keysym ) );
    }
  }
}

XEventLoop::~XEventLoop()
{
  {
    MutexLock x( &mutex );
    live = false;
  }

  display->makeevent();

  pthread_join( thread_handle, NULL );
}
