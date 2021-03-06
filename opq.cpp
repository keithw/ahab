#ifndef OPQ_CPP
#define OPQ_CPP

#include <pthread.h>

#include "opq.hpp"
#include "exceptions.hpp"
#include "mutexobj.hpp"

#include <typeinfo>
#include <stdio.h>

template <class T>
Queue<T>::Queue( int s_max_size )
  : count( 0 ),
    max_size( s_max_size ),
    head( NULL ),
    tail( NULL ),
    output( NULL ),
    enqueue_callback( NULL )
{
  unixassert( pthread_mutex_init( &mutex, NULL ) );
  unixassert( pthread_cond_init( &write_activity, NULL ) );
  unixassert( pthread_cond_init( &read_activity, NULL ) );
}

template <class T>
void Queue<T>::set_enqueue_callback( void (*s_enqueue_callback)(void *obj),
				     void *s_obj)
{
  MutexLock x( &mutex );
  enqueue_callback = s_enqueue_callback;
  obj = s_obj;
}

template <class T>
Queue<T>::~Queue()
{
  {
    MutexLock x( &mutex );

    QueueElement<T> *ptr = head;

    while ( ptr ) {
      T *op = ptr->element;
      QueueElement <T> *next = ptr->next;
      delete op;
      delete ptr;
      ptr = next;
    }

    unixassert( pthread_cond_destroy( &read_activity ) );
    unixassert( pthread_cond_destroy( &write_activity ) );
  }

  unixassert( pthread_mutex_destroy( &mutex ) );
}

template <class T>
QueueElement<T> *Queue<T>::enqueue( T *h )
{
  MutexLock x( &mutex );

  if ( output ) {
    ahabassert( !head );
    ahabassert( !tail );
    return output->enqueue( h );
  }

  while ( max_size && (count >= max_size) ) {
    unixassert( pthread_cond_wait( &read_activity, &mutex ) );
  }

  QueueElement<T> *op = new QueueElement<T>( h );
  op->prev = NULL;
  op->next = head;

  if ( head ) {
    head->prev = op;
    head = op;
  } else {
    head = tail = op;
  }

  count++;

  unixassert( pthread_cond_signal( &write_activity ) );

  if ( enqueue_callback ) {
    (*enqueue_callback)(obj);
  }

  return op;
}

template <class T>
QueueElement<T> *Queue<T>::leapfrog_enqueue( T *h,
						      T *leapfrog_type )
{
  MutexLock x( &mutex );

  if ( output ) {
    ahabassert( !head );
    ahabassert( !tail );
    return output->leapfrog_enqueue( h, leapfrog_type );
  }

  while ( max_size && (count >= max_size) ) {
    unixassert( pthread_cond_wait( &read_activity, &mutex ) );
  }

  QueueElement<T> *op = new QueueElement<T>( h );
  QueueElement<T> *ptr = tail;

  while ( (ptr != NULL) && ( typeid( ptr->element ) !=
			     typeid( h ) ) ) {
    ptr = tail->prev;
  }
  
  if ( ptr ) {
    op->next = ptr->next;
    ptr->next = op;
    op->prev = ptr;
    if ( op->next ) {
      op->next->prev = op;
    } else {
      tail = op;
    }
  } else {
    head = tail = op;
    op->prev = op->next = NULL;
  }

  count++;

  unixassert( pthread_cond_signal( &write_activity ) );

  if ( enqueue_callback ) {
    (*enqueue_callback)(obj);
  }

  return op;
}

template <class T>
void Queue<T>::remove_specific( QueueElement<T> *op )
{
  MutexLock x( &mutex );

  ahabassert( count > 0 );

  if ( op->prev ) {
    op->prev->next = op->next;
  } else {
    head = op->next;
  }

  if ( op->next ) {
    op->next->prev = op->prev;
  } else {
    tail = op->prev;
  }

  delete op; 
  count--;
  unixassert( pthread_cond_signal( &read_activity ) );  
}

template <class T>
T *Queue<T>::dequeue( bool wait )
{
  QueueElement<T> *ret_elem;
  T *ret;
  MutexLock x( &mutex );

  if ( (!wait) && (count == 0 ) ) {
    return NULL;
  }

  while ( count == 0 ) {
    unixassert( pthread_cond_wait( &write_activity, &mutex ) );      
  }

  ret_elem = tail;
  tail = tail->prev;
  if ( tail ) {
    tail->next = NULL;
  } else {
    ahabassert( count == 1 );
    head = NULL;
  }

  ret = ret_elem->element;
  delete ret_elem;
  count--;

  unixassert( pthread_cond_signal( &read_activity ) );

  return ret;
}

template <class T>
void Queue<T>::flush( void )
{
  MutexLock x( &mutex );

  if ( output ) {
    ahabassert( !head );
    ahabassert( !tail );
    return output->flush();
  }

  QueueElement<T> *ptr = head;

  while ( ptr ) {
    T *op = ptr->element;
    QueueElement <T> *next = ptr->next;
    delete op;
    delete ptr;
    ptr = next;
  }

  head = tail = NULL;
  count = 0;
}

template <class T>
void Queue<T>::flush_type( T *h )
{
  MutexLock x( &mutex );

  if ( output ) {
    ahabassert( !head );
    ahabassert( !tail );
    return output->flush_type( h );
  }

  QueueElement<T> *ptr = head;

  while ( ptr ) {
    T *op = ptr->element;

    bool deleting = ( typeid( op ) == typeid( h ) );

    QueueElement <T> *next = ptr->next;

    if ( deleting ) {
      if ( ptr->prev ) {
	ptr->prev->next = ptr->next;
      } else {
	ahabassert( ptr == head );
	head = ptr->next;
      }

      if ( ptr->next ) {
	ptr->next->prev = ptr->prev;
      } else {
	assert( ptr == tail );
	tail = ptr->prev;
      }

      delete op;
      delete ptr;

      count--;
    }

    ptr = next;
  }
}

template <class T>
void Queue<T>::hookup( Queue<T> *s_output )
{
  MutexLock x( &mutex );

  ahabassert( !output );

  output = s_output;

  /* move anything already in our queue */
  QueueElement<T> *ptr = head;

  while ( ptr ) {
    T *op = ptr->element;
    QueueElement <T> *next = ptr->next;

    output->enqueue( op ); /* this can block for a long time */

    delete ptr;
    ptr = next;
  }

  head = tail = NULL;
  count = 0;
}

#endif
