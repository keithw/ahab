#ifndef DETACH_ATTR_HPP
#define DETACH_ATTR_HPP

#include <pthread.h>

#include "exceptions.hpp"

class DetachAttributes {
public:
  pthread_attr_t attr;

  DetachAttributes( int detachstate ) {
    unixassert( pthread_attr_init( &attr ) );
    unixassert( pthread_attr_setdetachstate( &attr, detachstate ) );
  }
};

static DetachAttributes DetachedThread( PTHREAD_CREATE_DETACHED );

#endif
