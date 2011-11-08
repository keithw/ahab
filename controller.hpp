#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <gtkmm-2.4/gtkmm.h>
#include <pthread.h>

#include "decoderop.hpp"
#include "controllerop.hpp"

class ControllerState {
public:
  Glib::Dispatcher *move_slider;
  Gtk::VBox *grid;
  Gtk::HScale *scale;
  Gtk::CheckButton *fs_button;
};

class Controller {
private:
  Gtk::Main *main;
  Gtk::Window *window;

  pthread_mutex_t mutex;
  pthread_t thread_handle;

  void on_button( void );
  bool on_changed_value( Gtk::ScrollType scroll, double new_value );
  void shutdown( void ) { main->quit(); }
  void move( void );

  Glib::Dispatcher *quit_signal;
  ControllerState state;

  Queue<DecoderOperation> opq;
  Queue<ControllerOperation> inputq;

  int num_frames;

public:
  Controller( uint s_num_frames );
  ~Controller();

  void loop( void );
  Queue<DecoderOperation> *get_queue() { return &opq; }
  Queue<ControllerOperation> *get_input_queue() { return &inputq; }

  void tick_move_slider( void );
};

#endif
