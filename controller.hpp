#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <gtkmm-2.4/gtkmm.h>
#include <pthread.h>

#include "decoderop.hpp"

class Controller {
private:
  Gtk::Main *main;
  Gtk::Window *window;
  Gtk::HScale *scale;

  pthread_mutex_t mutex;
  pthread_t thread_handle;

  bool on_changed_value( Gtk::ScrollType scroll, double new_value );
  void shutdown( void ) { main->quit(); }

  Glib::Dispatcher *quit_signal;

  OperationQueue<DecoderOperation> opq;

  int num_frames;

public:
  Controller( uint s_num_frames );
  ~Controller();

  void loop( void );
  OperationQueue<DecoderOperation> *get_queue() { return &opq; }
};

#endif
