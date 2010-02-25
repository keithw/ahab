#ifndef ES_HPP
#define ES_HPP

/* MPEG-2 Video Elementary Stream */

#include <stdint.h>

#include "mpegheader.hpp"
#include "bitreader.hpp"
#include "file.hpp"

const int BLOCK = 65536;
const int LARGEST_HEADER = 260;
const int START_CODE_LENGTH = 3;

class MPEGHeader;

class ES {
private:
  File *file;

  off_t startfinder( off_t start,
		     void (*progress)( off_t size, off_t location ),
		     bool (ES::*todo)( uint8_t *buffer, off_t location,
				       size_t len ) );

  bool first_sequence( uint8_t *buf, off_t location, size_t len );
  bool add_header( uint8_t *buf, off_t location, size_t len );

  MPEGHeader *first_header;
  MPEGHeader *last_header;

  Sequence *seq;

  void number_pictures( void );

  uint num_pictures;
  Picture **coded_picture;
  Picture **displayed_picture;

  uint64_t duration_numer, duration_denom;
  double duration;

  BufferPool *pool;

public:
  ES( File *s_file, void (*progress)( off_t size, off_t location ) );
  ~ES();

  uint get_num_pictures( void ) { return num_pictures; }
  double get_duration( void ) { return duration; }

  Picture *get_picture_displayed( uint n ) { ahabassert( n < num_pictures ); return displayed_picture[ n ]; }
  Picture *get_picture_coded( uint n ) { ahabassert( n < num_pictures ); return coded_picture[ n ]; }

  Sequence *get_sequence( void ) { return seq; }
  File *get_file( void ) { return file; }

  BufferPool *get_pool( void ) { return pool; }
};

#endif
