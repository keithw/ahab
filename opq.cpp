#include <pthread.h>

#include "opq.hpp"
#include "exceptions.hpp"
#include "mutexobj.hpp"

#include <typeinfo>

template <class T>
OperationQueue<T>::OperationQueue( int s_max_size )
  : count( 0 ),
    max_size( s_max_size ),
    head( NULL ),
    tail( NULL ),
    output( NULL )
{
  unixassert( pthread_mutex_init( &mutex, NULL ) );
  unixassert( pthread_cond_init( &write_activity, NULL ) );
  unixassert( pthread_cond_init( &read_activity, NULL ) );
}

template <class T>
OperationQueue<T>::~OperationQueue()
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
void OperationQueue<T>::enqueue( T *h )
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
}

template <class T>
void OperationQueue<T>::leapfrog_enqueue( T *h,
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
}

template <class T>
T *OperationQueue<T>::dequeue( bool wait )
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
    assert( count == 1 );
    head = NULL;
  }

  ret = ret_elem->element;
  delete ret_elem;
  count--;

  unixassert( pthread_cond_signal( &read_activity ) );

  return ret;
}

template <class T>
void OperationQueue<T>::flush( void )
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
void OperationQueue<T>::flush_type( T *h )
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
void OperationQueue<T>::hookup( OperationQueue<T> *s_output )
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
