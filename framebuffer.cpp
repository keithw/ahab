#include "framebuffer.hpp"
#include "mutexobj.hpp"

FrameQueue::FrameQueue( void )
{
  first = last = NULL;
  unixassert( pthread_mutex_init( &mutex, NULL ) );
}

void FrameQueue::add( Frame *frame )
{
  MutexLock x( &mutex );

  frame->prev = last;
  frame->next = NULL;

  if ( last ) {
    last->next = frame;
    last = frame;
  } else {
    first = last = frame;
  }
}

Frame *FrameQueue::remove( void )
{
  MutexLock x( &mutex );

  if ( first == NULL ) { /* empty */
    return NULL;
  }

  Frame *return_value = first;

  first = first->next;

  if ( first ) {
    first->prev = NULL;
  } else {
    last = NULL;
  }

  return_value->prev = return_value->next = NULL;

  return return_value;
}

void FrameQueue::remove_specific( Frame *frame )
{
  MutexLock x( &mutex );

  if ( frame->prev ) {
    frame->prev->next = frame->next;
  } else {
    first = frame->next;
  }

  if ( frame->next ) {
    frame->next->prev = frame->prev;
  } else {
    last = frame->prev;
  }

  frame->prev = frame->next = NULL;
}

BufferPool::BufferPool( uint s_num_frames, uint s_width, uint s_height )
{
  num_frames = s_num_frames;
  width = s_width;
  height = s_height;

  frames = new Frame *[ num_frames ];
  for ( uint i = 0; i < num_frames; i++ ) {
    frames[ i ] = new Frame( width, height );
    free.add( frames[ i ] );
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

Frame::Frame( uint s_width, uint s_height )
{
  width = s_width;
  height = s_height;
  buf = new uint8_t[ sizeof( uint8_t ) * (3 * width * height / 2) ];
  state = FREE;
  handle = NULL;
  unixassert( pthread_mutex_init( &mutex, NULL ) );
}

Frame::~Frame()
{
  delete[] buf;
  unixassert( pthread_mutex_destroy( &mutex ) );
}

void Frame::lock( FrameHandle *s_handle )
{
  MutexLock x( &mutex );

  ahabassert( handle == NULL );
  ahabassert( state == FREE );
  handle = s_handle;
  state = LOCKED;
}

void Frame::set_rendered( void )
{
  MutexLock x( &mutex );

  ahabassert( state == LOCKED );
  state = RENDERED;
}

void Frame::relock( void )
{
  MutexLock x( &mutex );

  ahabassert( state == FREEABLE );
  state = RENDERED;
}

void Frame::set_freeable( void )
{
  MutexLock x( &mutex );

  ahabassert( state == RENDERED );
  state = FREEABLE;
}

void Frame::free_locked( void )
{
  MutexLock x( &mutex );

  ahabassert( state == LOCKED );
  /* handle->set_frame( NULL ); */ /* handle takes care of this */
  handle = NULL;
  state = FREE;
}

void Frame::free( void )
{
  MutexLock x( &mutex );

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
  unixassert( pthread_mutex_init( &mutex, NULL ) );
}

void FrameHandle::increment_lockcount( void )
{
  MutexLock x( &mutex );

  if ( frame ) {
    if ( locks == 0 ) {
      ahabassert( frame->get_state() == FREEABLE );
      pool->remove_from_freeable( frame );
      frame->relock();
    }
    locks++;
  } else {
    ahabassert( locks == 0 );
    frame = pool->get_free_frame();
    frame->lock( this );
    locks++;
  }
}

void FrameHandle::decrement_lockcount( void )
{
  MutexLock x( &mutex );

  ahabassert( locks > 0 );
  locks--;
  if ( locks == 0 ) {
    if ( frame->get_state() == RENDERED ) {
      pool->make_freeable( frame );
      frame->set_freeable();
    } else if ( frame->get_state() == LOCKED ) {
      pool->make_free( frame );
      frame->free_locked();
      frame = NULL;
    } else {
      throw AhabException();
    }
  }
}

Frame *BufferPool::get_free_frame( void )
{
  MutexLock x( &mutex );

  Frame *first_free = free.remove();
  if ( first_free ) {
    return first_free;
  }

  Frame *first_freeable = freeable.remove();
  if ( !first_freeable ) {
    throw OutOfFrames();
  }
  first_freeable->free();

  return first_freeable;
}

void BufferPool::make_freeable( Frame *frame )
{
  MutexLock x( &mutex );
  freeable.add( frame );
}

void BufferPool::make_free( Frame *frame )
{
  MutexLock x( &mutex );
  free.add( frame );
}

void BufferPool::remove_from_freeable( Frame *frame )
{
  MutexLock x( &mutex );
  freeable.remove_specific( frame );
}

void FrameHandle::set_frame( Frame *s_frame )
{
  MutexLock x( &mutex );
  ahabassert( locks == 0 );
  frame = s_frame;
}
