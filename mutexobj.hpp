#ifndef MUTEXOBJ_HPP
#define MUTEXOBJ_HPP

#include <pthread.h>
#include "exceptions.hpp"

class MutexLock {
private:
  pthread_mutex_t *mutex;

public:
  MutexLock( pthread_mutex_t *s_mutex ) {
    mutex = s_mutex;
    unixassert( pthread_mutex_lock( mutex ) );
  }
  ~MutexLock() {
    unixassert( pthread_mutex_unlock( mutex ) );
  }
};

#endif
