#ifndef DECODEENGINE_HPP
#define DECODEENGINE_HPP

#include "opq.hpp"
#include "decoderjob.hpp"
#include "exceptions.hpp"

class ReadyThread {
public:
  pthread_t handle;
  Queue<DecoderJob> *opq;

  ReadyThread( pthread_t s_handle, Queue<DecoderJob> *s_opq )
    : handle( s_handle ), opq( s_opq )
  {}
};

class DecodeEngine {
private:
  pthread_mutex_t mutex;
  int thread_count;
  Queue<ReadyThread> threadq;

public:
  DecodeEngine()
    : thread_count( 0 ),
      threadq( 0 )
  {
    unixassert( pthread_mutex_init( &mutex, NULL ) );
  }

  ~DecodeEngine();

  void dispatch( DecoderJob *job );
};

#endif
