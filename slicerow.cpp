#include <pthread.h>

#include "slicerow.hpp"
#include "exceptions.hpp"
#include "mutexobj.hpp"

SliceRow::SliceRow( uint s_row, uint s_mb_height )
  : row( s_row ),
    mb_height( s_mb_height )
{
  unixassert( pthread_mutex_init( &mutex, NULL ) );
  unixassert( pthread_cond_init( &activity, NULL ) );
  state = SR_BLANK;
}

SliceRow::~SliceRow()
{
  unixassert( pthread_cond_destroy( &activity ) );
  unixassert( pthread_mutex_destroy( &mutex ) );
}

void SliceRow::init( int f_code_fv, int f_code_bv,
		     Picture *forward, Picture *backward )
{
  forward_highest_dependent_row = forward_lowest_dependent_row = -1;
  backward_highest_dependent_row = backward_lowest_dependent_row = -1;

  if ( forward && (f_code_fv != 15) ) {
    int diff;

    if ( f_code_fv == 1 ) {
      diff = 1;
    } else {
      diff = 1 << (f_code_fv - 2);
    }

    forward_highest_dependent_row = row - diff;
    if ( forward_highest_dependent_row < 0 ) forward_highest_dependent_row = 0;

    forward_lowest_dependent_row = row + diff;
    if ( forward_lowest_dependent_row >= (int)mb_height ) forward_lowest_dependent_row = mb_height - 1;
  }

  if ( backward && (f_code_bv != 15) ) {
    int diff;

    if ( f_code_bv == 1 ) {
      diff = 1;
    } else {
      diff = 1 << (f_code_bv - 2);
    }

    backward_highest_dependent_row = row - diff;
    if ( backward_highest_dependent_row < 0 ) backward_highest_dependent_row = 0;

    backward_lowest_dependent_row = row + diff;
    if ( backward_lowest_dependent_row >= (int)mb_height ) backward_lowest_dependent_row = mb_height - 1;
  }

  MutexLock x( &mutex );

  state = SR_READY;
}
