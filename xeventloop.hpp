#ifndef XEVENTLOOP_HPP
#define XEVENTLOOP_HPP

#include "decoderop.hpp"
#include "ogl.hpp"

class XEventLoop {
private:
  OperationQueue<DecoderOperation> opq;
  OpenGLDisplay *display;

  pthread_t thread_handle;

  pthread_mutex_t mutex;
  bool live;

public:
  XEventLoop( OpenGLDisplay *s_display );
  ~XEventLoop();

  void loop( void );
  OperationQueue<DecoderOperation> *get_queue() { return &opq; }
};

#endif
