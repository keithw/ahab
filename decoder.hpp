#ifndef DECODER_HPP
#define DECODER_HPP

#include <pthread.h>

#include "es.hpp"

class OpenGLDisplay;
class DecoderOperation;
class DisplayOperation;

#include "opq.hpp"
#include "decodeengine.hpp"

class DecoderState {
public:
  int current_picture;
  bool fullscreen;
  bool live;
  OpenGLDisplay *display;
  Queue<DisplayOperation> *oglq;
};

class Decoder {
private:
  DecoderState state;
  Queue<DecoderOperation> opq;
  pthread_t thread_handle;
  DecodeEngine engine;

  ES *stream;

  void decode_and_display( void );

public:
  Decoder( ES *s_stream, Queue<DisplayOperation> *s_oglq );
  ~Decoder();
  
  void loop();
  Queue<DecoderOperation> *get_queue() { return &opq; }
  void wait_shutdown( void );
};

#endif 
