#ifndef DECODER_HPP
#define DECODER_HPP

#include <pthread.h>

#include "es.hpp"

class OpenGLDisplay;
class DecoderOperation;
class DisplayOperation;

#include "opq.hpp"
#include "decodeengine.hpp"
#include "controllerop.hpp"

class DecoderState {
public:
  int current_picture;
  bool fullscreen;
  bool live;
  OpenGLDisplay *display;
  Queue<DisplayOperation> *oglq;
  Queue<ControllerOperation> outputq;
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
  Queue<ControllerOperation> *get_output_queue() { return &state.outputq; }
  void wait_shutdown( void );
};

#endif 
