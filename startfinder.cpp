#include <unistd.h>
#include <errno.h>
#include <typeinfo>

#include "es.hpp"

inline bool start_code( uint8_t *buf )
{
  return ( (buf[ 0 ] == 0) &&
	   (buf[ 1 ] == 0) &&
	   (buf[ 2 ] == 1) );
}

off_t ES::startfinder( off_t start,
		       void (*progress)( off_t size, off_t location ),
		       bool (ES::*todo)( uint8_t *buffer, off_t location,
					 size_t len ) )
{
  ssize_t maxread = BLOCK + START_CODE_LENGTH;
  off_t last_code = -1;
  bool keepgoing = true;
  off_t anchor = start;
  off_t filesize = file->get_filesize();

  while ( 1 ) {
    int len = maxread;
    if ( anchor + len > filesize ) {
      len = filesize - anchor;
    }

    if ( len < START_CODE_LENGTH ) {
      break;
    }

    int advance = len - START_CODE_LENGTH;

    MapHandle *chunk = file->map( anchor, len );
    uint8_t *buf = chunk->get_buf();

    /* Look through every byte of buffer */
    int i = 0;
    while ( i < len - LARGEST_HEADER ) {
      if ( start_code( buf + i ) ) {
	keepgoing = (this->*todo)( buf + i, anchor + i, len - i );
	if ( !keepgoing ) {
	  last_code = anchor + i;
	  break;
	}
      }
      i++;
    }
    
    if ( keepgoing ) {
      while ( i < len - START_CODE_LENGTH ) {
	if ( start_code( buf + i ) ) {
	  try {
	    keepgoing = (this->*todo)( buf + i, anchor + i, len - i );
	    if ( !keepgoing ) {
	      last_code = anchor + i;
	      break;
	    }
	  } catch ( NeedBits x ) {
	    if ( anchor + len == filesize ) {
	      keepgoing = false;
	      break;
	    } else {
	      advance = i;
	      break;
	    }
	  }
	}
	i++;
      }
    }

    delete chunk;

    progress( filesize, anchor + len );
    if ( !keepgoing ) break;
    if ( advance == 0 ) break;

    anchor += advance;
  }

  return last_code;
}

bool ES::first_sequence( uint8_t *buf, off_t location, size_t len )
{
  return (BitReader( buf, len ).readbits( 32 ) != 0x1b3);
}

bool ES::add_header( uint8_t *buf, off_t location, size_t len )
{
  BitReader br( buf, len );

  MPEGHeader *hdr = MPEGHeader::make( br, file );
  if ( hdr ) {
    if ( first_header == NULL ) {
      first_header = last_header = hdr;
    } else {
      last_header->set_next( hdr );
    }
    hdr->set_location( location );
    last_header = hdr;

    if ( (seq == NULL) && (typeid( *hdr ) == typeid( Sequence )) ) {
      seq = dynamic_cast<Sequence *>( hdr );
      ahabassert( seq );
    }
  }

  if ( typeid( *hdr ) == typeid( SequenceEnd ) ) {
    return false;
  } else {
    return true;
  }
}
