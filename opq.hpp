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
class Queue
{
private:
  uint num_ops;

  pthread_mutex_t mutex;
  pthread_cond_t  read_activity;
  pthread_cond_t  write_activity;

  int count, max_size;
  QueueElement<T> *head, *tail;

  Queue<T> *output;

  void (*enqueue_callback)(void *obj);
  void *obj;

public:
  Queue( int s_max_size );
  Queue() { Queue( 0 ); }
  ~Queue();

  QueueElement<T> *enqueue( T *h );
  QueueElement<T> *leapfrog_enqueue( T *h, T *leapfrog_type );
  void remove_specific( QueueElement<T> *op );

  T *dequeue( bool wait );

  void flush_type( T *h );
  void flush( void );  

  int get_count( void ) { MutexLock x( &mutex ); return count; }

  void hookup( Queue<T> *s_output );

  void set_enqueue_callback( void (*s_enqueue_callback)(void *obj),
			     void *s_obj );
};

#endif
