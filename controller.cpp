#include "controller.hpp"
#include "exceptions.hpp"
#include "decoderop.hpp"
#include "mutexobj.hpp"
#include "controllerop.hpp"

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
    inputq( 0 ),
    num_frames( s_num_frames )
{
  unixassert( pthread_mutex_init( &mutex, NULL ) );
  pthread_create( &thread_handle, NULL, thread_helper, this );
}

void Controller::move( void ) {
  ControllerOperation *op = inputq.dequeue( false );
  if ( op ) {
    op->execute( state );
    delete op;
  }
}

void tick_move_slider_helper( void *obj )
{
  Controller *me = static_cast<Controller *>( obj );
  ahabassert( me );
  me->tick_move_slider();
}

void Controller::tick_move_slider( void )
{
  MutexLock x( &mutex );
  if ( state.move_slider ) {
    (*state.move_slider)();
  }
}

void Controller::loop( void )
{
  main = new Gtk::Main( 0, NULL );
  window = new Gtk::Window;

  {
    MutexLock x( &mutex );
    quit_signal = new Glib::Dispatcher;
    state.move_slider = new Glib::Dispatcher;

    window->set_default_size( 800, 50 );
    window->set_title( "Ahab Controller" );

    state.grid = new Gtk::VBox();
    window->add( *state.grid );
    state.grid->set_homogeneous( false );

    state.fs_button = new Gtk::CheckButton( "fullscreen" );

    state.grid->add( *state.fs_button );
    state.fs_button->signal_toggled().connect( sigc::mem_fun( this, &Controller::on_button ) );

    state.scale = new Gtk::HScale( 0, num_frames, 1 );
    state.grid->add( *state.scale );

    state.scale->set_update_policy( Gtk::UPDATE_CONTINUOUS );
    state.scale->set_digits( 0 );
    state.scale->set_value_pos( Gtk::POS_TOP );
    state.scale->set_draw_value();

    state.scale->signal_change_value().connect( sigc::mem_fun( this, &Controller::on_changed_value ) );

    quit_signal->connect( sigc::mem_fun( this, &Controller::shutdown ) );
    state.move_slider->connect( sigc::mem_fun( this, &Controller::move ) );

    inputq.set_enqueue_callback( tick_move_slider_helper, this );

    state.grid->show();
    state.fs_button->show();
    state.scale->show();
  }

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

  delete state.fs_button;
  delete state.scale;
  delete state.grid;
  delete window;
  delete main;
}

void Controller::on_button( void )
{
  opq.enqueue( new XKey( 'f' ) );
}

bool Controller::on_changed_value( Gtk::ScrollType, double new_value )
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
