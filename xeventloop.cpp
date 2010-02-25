#include <pthread.h>
#include <signal.h>
#include <stdio.h>

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
  : opq( 0 ),
    display( s_display ),
    live( true )
{
  unixassert( pthread_mutex_init( &mutex, NULL ) );
  pthread_create( &thread_handle, NULL, thread_helper, this );  
}

void XEventLoop::loop( void )
{
  while ( 1 ) {
    int key = display->getevent( true );

    {
      MutexLock x( &mutex );
      if ( !live ) {
	return;
      }
    }

    if ( key ) {
      XKey *op = new XKey( key );
      try {
	opq.enqueue( op );
      } catch ( UnixAssertError *e ) {
	return;
      }
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
