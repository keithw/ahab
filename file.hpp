#ifndef FILE_HPP
#define FILE_HPP

#include <stdint.h>
#include <sys/mman.h>

class MapHandle {
  friend class File;

private:
  uint8_t *user_buf;
  uint8_t *mmap_buf;
  size_t maplen;
  size_t userlen;
  MapHandle( uint8_t *s_user, uint8_t *s_mmap, size_t s_maplen, size_t s_userlen )
  {
    user_buf = s_user; mmap_buf = s_mmap; maplen = s_maplen; userlen = s_userlen;
  }

public:
  ~MapHandle();

  uint8_t *get_buf( void ) { return user_buf; }
  size_t get_len( void ) { return userlen; }
};

class File {
private:
  int fd;
  off_t filesize;

public:
  File( char *filename );
  ~File();

  MapHandle *map( off_t offset, size_t len );
  off_t get_filesize( void ) { return filesize; }
};

#endif
