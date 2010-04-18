#ifndef FRAMEBUFFER_HPP
#define FRAMEBUFFER_HPP

#include "mpegheader.hpp"
#include "exceptions.hpp"
#include "slicerow.hpp"
#include "opq.hpp"

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
  pthread_cond_t activity;

  void set_frame( Frame *s_frame );

public:
  void increment_lockcount( void );
  void decrement_lockcount( void );

  Frame *get_frame( void ) { ahabassert( frame ); return frame; }
  Picture *get_picture( void ) { return pic; }

  FrameHandle( BufferPool *s_pool, Picture *s_pic );
  ~FrameHandle();

  void wait_rendered( void );
};

class BufferPool
{
private:
  uint num_frames, width, height;
  Frame **frames;

  Queue<Frame> free;
  Queue<Frame> freeable;

  pthread_mutex_t mutex;

public:
  BufferPool( uint s_num_frames, uint mb_width, uint mb_height );
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
private:
  uint width, height;
  uint8_t *buf;
  FrameState state;

  FrameHandle *handle;

  Frame *prev, *next;

  pthread_mutex_t mutex;
  pthread_cond_t activity;

  SliceRow **slicerow;

  QueueElement<Frame> *queue_element;

public:
  Frame( uint mb_width, uint mb_height );
  ~Frame();

  uint8_t *get_buf( void ) { return buf; }
  uint8_t *get_y( void ) { return buf; }
  uint8_t *get_cb( void ) { return buf + width * height; }
  uint8_t *get_cr( void ) { return buf + width * height + width * height / 4; }

  void lock( FrameHandle *s_handle,
	     int f_code_fv, int f_code_bv,
	     Picture *forward, Picture *backward );
  void set_rendered( void );
  void set_freeable( void );
  void relock( void );
  void free( void );
  void free_locked( void );

  FrameState get_state( void ) { return state; }

  void wait_rendered( void );

  SliceRow *get_slicerow( uint row ) { return slicerow[ row ]; }

  void set_element( QueueElement<Frame> *s_element ) { queue_element = s_element; }
  QueueElement<Frame> *get_element( void ) { return queue_element; }
};

#endif
