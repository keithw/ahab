#ifndef OPQ_HPP
#define OPQ_HPP

#include <pthread.h>
#include <sys/types.h>

#include "mutexobj.hpp"

template <class T>
class QueueElement
{
public:
  QueueElement<T> *prev, *next;
  T *element;

  QueueElement<T>( T *s ) { element = s; }
};

template <class T>
class OperationQueue
{
private:
  uint num_ops;

  pthread_mutex_t mutex;
  pthread_cond_t  read_activity;
  pthread_cond_t  write_activity;

  int count, max_size;
  QueueElement<T> *head, *tail;

  OperationQueue<T> *output;

public:
  OperationQueue( int s_max_size );
  ~OperationQueue();
  QueueElement<T> *enqueue( T *h );
  QueueElement<T> *leapfrog_enqueue( T *h, T *leapfrog_type );
  void remove_specific( QueueElement<T> *op );

  T *dequeue( bool wait );

  void flush_type( T *h );
  void flush( void );  

  int get_count( void ) { MutexLock x( &mutex ); return count; }

  void hookup( OperationQueue<T> *s_output );
};

#endif
