#include "controller.hpp"
#include "exceptions.hpp"
#include "decoderop.hpp"
#include "mutexobj.hpp"

#include <stdio.h>

static void *thread_helper( void *controller )
{
  Controller *me = static_cast<Controller *>( controller );
  ahabassert( me );
  me->loop();
  return NULL;
}

Controller::Controller( uint s_num_frames )
  : quit_signal( NULL ),
    opq( 0 ),
    num_frames( s_num_frames )
{
  unixassert( pthread_mutex_init( &mutex, NULL ) );
  pthread_create( &thread_handle, NULL, thread_helper, this );
}

void Controller::loop( void )
{
  main = new Gtk::Main( 0, NULL );
  window = new Gtk::Window;

  {
    MutexLock x( &mutex );
    quit_signal = new Glib::Dispatcher;
  }

  window->set_default_size( 600, 50 );
  window->set_title( "Ahab Controller" );

  scale = new Gtk::HScale( 0, num_frames, 1 );
  window->add( *scale );

  scale->set_update_policy( Gtk::UPDATE_CONTINUOUS );
  scale->set_digits( 0 );
  scale->set_value_pos( Gtk::POS_TOP );
  scale->set_draw_value();

  scale->signal_change_value().connect( sigc::mem_fun( this, &Controller::on_changed_value ) );

  quit_signal->connect( sigc::mem_fun( this, &Controller::shutdown ) );

  scale->show();

  main->run( *window );

  {
    MutexLock x( &mutex );
    delete quit_signal;
    quit_signal = NULL;
  }

  DecoderShutDown *op = new DecoderShutDown();

  try {
    opq.enqueue( op );
  } catch ( UnixAssertError *e ) {}

  delete scale;
  delete window;
  delete main;
}

bool Controller::on_changed_value( Gtk::ScrollType scroll, double new_value )
{
  int cur_frame = lround( new_value );

  if ( cur_frame < 0 ) cur_frame = 0;
  if ( cur_frame >= num_frames ) cur_frame = num_frames - 1;

  SetPictureNumber *op = new SetPictureNumber( cur_frame );
  opq.flush_type( op );
  opq.enqueue( op );

  return true;
}

Controller::~Controller()
{
  {
    MutexLock x( &mutex );
    if ( quit_signal ) {
      (*quit_signal)();
    }
  }
  unixassert( pthread_join( thread_handle, NULL ) );
  unixassert( pthread_mutex_destroy( &mutex ) );
}
