#include <pthread.h>

#include "decodeengine.hpp"
#include "decoderjob.hpp"
#include "picture.hpp"
#include "mutexobj.hpp"

static void *job_runner( void *s_threadq )
{
  Queue<ReadyThread> *threadq = (Queue<ReadyThread> *)s_threadq;
  Queue<DecoderJob> opq( 0 );

  ReadyThread me( pthread_self(), &opq );

  while ( 1 ) {
    threadq->enqueue( &me );
    DecoderJob *job = opq.dequeue( true );
    job->execute();
    delete job;
  }

  return NULL;
}

void DecodeEngine::dispatch( DecoderJob *job )
{
  ReadyThread *thread = threadq.dequeue( false );
  if ( thread ) {
    thread->opq->enqueue( job );
  } else {
    pthread_t new_thread;
    {
      MutexLock x( &mutex );
      pthread_create( &new_thread, NULL, job_runner, &threadq );
      thread_count++;
    }
    thread = threadq.dequeue( true );
    ahabassert( thread );
    thread->opq->enqueue( job );
  }
}

DecodeEngine::~DecodeEngine()
{
  DecoderJob *shutdown = new DecoderJobShutDown();
  for ( int i = 0; i < thread_count; i++ ) {
    ReadyThread *thread = threadq.dequeue( true );
    pthread_t handle = thread->handle;
    thread->opq->enqueue( shutdown );
    unixassert( pthread_join( handle, NULL ) );
  }
  delete shutdown;
  unixassert( pthread_mutex_destroy( &mutex ) );
}
