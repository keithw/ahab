#ifndef SLICEROW_HPP
#define SLICEROW_HPP

#include <pthread.h>
#include "mpegheader.hpp"
#include "mutexobj.hpp"
#include "exceptions.hpp"

enum SliceRowState { SR_BLANK, SR_READY, SR_LOCKED, SR_RENDERED };

class SliceRow
{
private:
  uint row;
  uint mb_height;

  int forward_highest_dependent_row, forward_lowest_dependent_row;
  int backward_highest_dependent_row, backward_lowest_dependent_row;

  pthread_mutex_t mutex;
  pthread_cond_t activity;
  SliceRowState state;

public:
  SliceRow( uint s_row, uint s_mb_height );
  ~SliceRow();

  void init( int f_code_fv, int f_code_bv, Picture *forward, Picture *backward );

  SliceRowState lock( void ) {
    MutexLock x( &mutex );
    SliceRowState return_value = state;
    if ( state != SR_READY ) return return_value;
    state = SR_LOCKED;
    return return_value;
  }

  void set_rendered( void )
  {
    MutexLock x( &mutex );
    state = SR_RENDERED;
    unixassert( pthread_cond_broadcast( &activity ) );
  }

  void wait_rendered( void )
  {
    MutexLock x( &mutex );

    while ( state != SR_RENDERED ) {
      pthread_cond_wait( &activity, &mutex );
    }
  }

  void set_blank( void ) { MutexLock x( &mutex ); state = SR_BLANK; }

  SliceRowState get_state( void ) { MutexLock x( &mutex ); return state; }

  int get_forward_highrow( void ) { return forward_highest_dependent_row; }
  int get_forward_lowrow( void ) { return forward_lowest_dependent_row; }
  
  int get_backward_highrow( void ) { return backward_highest_dependent_row; }
  int get_backward_lowrow( void ) { return backward_lowest_dependent_row; }
};

#endif
