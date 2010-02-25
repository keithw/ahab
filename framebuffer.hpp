#ifndef FRAMEBUFFER_HPP
#define FRAMEBUFFER_HPP

#include "mpegheader.hpp"
#include "exceptions.hpp"

#include <stdint.h>
#include <pthread.h>

class Frame;
class BufferPool;

class FrameHandle
{
  friend class Frame;

private:
  BufferPool *pool;
  Picture *pic;
  Frame *frame;
  int locks;

  pthread_mutex_t mutex;

  void set_frame( Frame *s_frame );

public:
  void increment_lockcount( void );
  void decrement_lockcount( void );

  Frame *get_frame( void ) { return frame; }
  Picture *get_picture( void ) { return pic; }

  FrameHandle( BufferPool *s_pool, Picture *s_pic );
  ~FrameHandle() { unixassert( pthread_mutex_destroy( &mutex ) ); }
};

class FrameQueue
{
private:
  Frame *first, *last;

  pthread_mutex_t mutex;

public:
  FrameQueue( void );
  void add( Frame *frame );
  Frame *remove( void );
  void remove_specific( Frame *frame );
  ~FrameQueue() { unixassert( pthread_mutex_destroy( &mutex ) ); };
};

class BufferPool
{
private:
  uint num_frames, width, height;
  Frame **frames;

  FrameQueue free;
  FrameQueue freeable;

  pthread_mutex_t mutex;

public:
  BufferPool( uint s_num_frames, uint s_width, uint s_height );
  ~BufferPool();

  FrameHandle *make_handle( Picture *pic ) { return new FrameHandle( this, pic ); }
  Frame *get_free_frame( void );
  void make_freeable( Frame *frame );
  void make_free( Frame *frame );
  void remove_from_freeable( Frame *frame );
};

enum FrameState { FREE, LOCKED, RENDERED, FREEABLE };

class Frame
{
  friend class FrameQueue;

private:
  uint width, height;
  uint8_t *buf;
  FrameState state;

  FrameHandle *handle;

  Frame *prev, *next;

  pthread_mutex_t mutex;

public:
  Frame( uint s_width, uint s_height );
  ~Frame();

  uint8_t *get_buf( void ) { return buf; }
  uint8_t *get_y( void ) { return buf; }
  uint8_t *get_cb( void ) { return buf + width * height; }
  uint8_t *get_cr( void ) { return buf + width * height + width * height / 4; }

  void lock( FrameHandle *s_handle );
  void set_rendered( void );
  void set_freeable( void );
  void relock( void );
  void free( void );
  void free_locked( void );

  FrameState get_state( void ) { return state; }
};

#endif
