#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "file.hpp"
#include "exceptions.hpp"

File::File( char *filename )
{
  /* Open file */
  fd = open( filename, O_RDONLY );
  if ( fd < 0 ) {
    perror( "open" );
    throw UnixError( errno );
  }

  /* Get size of file */
  struct stat thestat;

  if ( fstat( fd, &thestat ) < 0 ) {
    perror( "fstat" );
    throw UnixError( errno );
  }

  filesize = thestat.st_size;
}

File::~File()
{
  if ( close( fd ) < 0 ) {
    perror( "close" );
    throw UnixError( errno );
  }
}

MapHandle *File::map( off_t offset, size_t len )
{
  long page = sysconf( _SC_PAGE_SIZE );

  off_t mmap_offset = offset & ~(page - 1);

  uint8_t *mbuf = (uint8_t *)mmap( NULL, len + offset - mmap_offset,
				   PROT_READ,
				   MAP_SHARED, fd, mmap_offset );
  if ( mbuf == MAP_FAILED ) {
    perror( "mmap" );
    throw UnixError( errno );
  }

  uint8_t *buf = mbuf + offset - mmap_offset;

  return new MapHandle( buf, mbuf, len + offset - mmap_offset, len );
}

MapHandle::~MapHandle()
{
  if ( munmap( mmap_buf, maplen ) < 0 ) {
    perror( "munmap" );
    throw UnixError( errno );
  }
}
