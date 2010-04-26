#include "framebuffer.hpp"
#include "mutexobj.hpp"
#include "picture.hpp"
#include "framequeue.hpp"

BufferPool::BufferPool( uint s_num_frames, uint mb_width, uint mb_height )
  : free( 0 ), freeable( 0 )
{
  num_frames = s_num_frames;
  width = 16 * mb_width;
  height = 16 * mb_height;

  frames = new Frame *[ num_frames ];
  for ( uint i = 0; i < num_frames; i++ ) {
    frames[ i ] = new Frame( this, mb_width, mb_height );
    free.enqueue( frames[ i ] );
  }

  unixassert( pthread_mutex_init( &mutex, NULL ) );
}

BufferPool::~BufferPool()
{
  for ( uint i = 0; i < num_frames; i++ ) {
    Frame *frame = frames[ i ];
    delete frame;
  }
  delete[] frames;

  unixassert( pthread_mutex_destroy( &mutex ) );
}

Frame::Frame( BufferPool *s_pool, uint mb_width, uint mb_height )
{
  pool = s_pool;
  width = 16 * mb_width;
  height = 16 * mb_height;
  buf = new uint8_t[ sizeof( uint8_t ) * (3 * width * height / 2) ];
  state = FREE;
  handle = NULL;
  unixassert( pthread_cond_init( &activity, NULL ) );

  slicerow = new SliceRow *[ mb_height ];

  for ( uint i = 0; i < mb_height; i++ ) {
    slicerow[ i ] = new SliceRow( i, mb_height );
  }
}

Frame::~Frame()
{
  delete[] buf;

  for ( uint i = 0; i < height / 16; i++ ) {
    delete slicerow[ i ];
  }

  delete[] slicerow;

  unixassert( pthread_cond_destroy( &activity ) );
}

void Frame::lock( FrameHandle *s_handle,
		  int f_code_fv, int f_code_bv,
		  Picture *forward, Picture *backward )
{
  ahabassert( handle == NULL );
  ahabassert( state == FREE );
  handle = s_handle;
  state = LOCKED;

  for ( uint i = 0; i < height / 16; i++ ) {
    slicerow[ i ]->init( f_code_fv, f_code_bv, forward, backward );
  }
}

void Frame::set_rendered( void )
{
  MutexLock x( pool->get_mutex() );

  ahabassert( state == LOCKED );
  state = RENDERED;
  pthread_cond_broadcast( &activity );
}

void Frame::relock( void )
{
  ahabassert( state == FREEABLE );
  state = RENDERED;
  pthread_cond_broadcast( &activity );
}

void Frame::set_freeable( void )
{
  ahabassert( state == RENDERED );
  state = FREEABLE;
}

void Frame::free_locked( void )
{
  ahabassert( state == LOCKED );
  /* handle->set_frame( NULL ); */ /* handle takes care of this */
  handle = NULL;
  state = FREE;
}

void Frame::free( void )
{
  ahabassert( state == FREEABLE );
  handle->set_frame( NULL );
  handle = NULL;
  state = FREE;
}

FrameHandle::FrameHandle( BufferPool *s_pool, Picture *s_pic )
{
  pool = s_pool;
  pic = s_pic;
  frame = NULL;
  locks = 0;
  unixassert( pthread_cond_init( &activity, NULL ) );
}

FrameHandle::~FrameHandle()
{
  unixassert( pthread_cond_destroy( &activity ) );
}

void FrameHandle::increment_lockcount( void )
{
  MutexLock x( pool->get_mutex() );

  if ( frame ) {
    if ( locks == 0 ) {
      ahabassert( frame->get_state() == FREEABLE );
      pool->remove_from_freeable( frame );
      frame->relock();
    }
    locks++;
  } else {
    ahabassert( locks == 0 );
    while ( (frame = pool->get_free_frame()) == NULL ) {
      pool->wait();
    }
    frame->lock( this, pic->get_f_code_fv(), pic->get_f_code_bv(),
		 pic->get_forward(), pic->get_backward() );
    locks++;
    pthread_cond_broadcast( &activity );
  }
}

bool FrameHandle::increment_lockcount_if_renderable( void )
{
  MutexLock x( pool->get_mutex() );

  if ( !frame ) return false;

  if ( locks == 0 ) {
    ahabassert( frame->get_state() == FREEABLE );
    pool->remove_from_freeable( frame );
    frame->relock();
    ahabassert( frame->get_state() == RENDERED );
    locks++;
    return true;
  } else if ( frame->get_state() == RENDERED ) {
    locks++;
    return true;
  }

  return false;
}

void FrameHandle::decrement_lockcount( void )
{
  MutexLock x( pool->get_mutex() );

  ahabassert( locks > 0 );
  locks--;
  if ( locks == 0 ) {
    if ( frame->get_state() == RENDERED ) {
      pool->make_freeable( frame );
      frame->set_freeable();
      pool->signal();
    } else if ( frame->get_state() == LOCKED ) {
      pool->make_free( frame );
      frame->free_locked();
      frame = NULL;
      pool->signal();
    } else {
      throw AhabException();
    }
  }
}

Frame *BufferPool::get_free_frame( void )
{
  Frame *first_free = free.dequeue( false );
  if ( first_free ) {
    return first_free;
  }

  Frame *first_freeable = freeable.dequeue( false );
  if ( first_freeable ) {
    first_freeable->free();
    return first_freeable;
  } else {
    return NULL;
  }
}

void BufferPool::make_freeable( Frame *frame )
{
  frame->set_element( freeable.enqueue( frame ) );
}

void BufferPool::make_free( Frame *frame )
{
  free.enqueue( frame );
}

void BufferPool::remove_from_freeable( Frame *frame )
{
  freeable.remove_specific( frame->get_element() );
}

void FrameHandle::set_frame( Frame *s_frame )
{
  ahabassert( locks == 0 );
  frame = s_frame;
  pthread_cond_broadcast( &activity );
}

void Frame::wait_rendered( void )
{
  while ( state != RENDERED ) {
    pthread_cond_wait( &activity, pool->get_mutex() );
  }
}

void FrameHandle::wait_rendered( void )
{
  MutexLock x( pool->get_mutex() );

  while ( !frame ) {
    pthread_cond_wait( &activity, pool->get_mutex() );
  }
  /* now we have a frame and our mutex is locked so it can't be taken away */

  frame->wait_rendered();
}
