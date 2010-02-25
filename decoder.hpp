#ifndef DECODER_HPP
#define DECODER_HPP

#include <pthread.h>

#include "es.hpp"

class OpenGLDisplay;
class DecoderOperation;
class DisplayOperation;

#include "opq.hpp"

class DecoderState {
public:
  int current_picture;
  bool fullscreen;
  bool live;
  OpenGLDisplay *display;
  OperationQueue<DisplayOperation> *oglq;
};

class Decoder {
private:
  DecoderState state;
  OperationQueue<DecoderOperation> opq;
  pthread_t thread_handle;

  ES *stream;

  void decode_and_display( void );

public:
  Decoder( ES *s_stream, OperationQueue<DisplayOperation> *s_oglq );
  ~Decoder();
  
  void loop();
  OperationQueue<DecoderOperation> *get_queue() { return &opq; }
  void wait_shutdown( void );
};

#endif 
