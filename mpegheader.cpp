#include <stdint.h>
#include <stdio.h>

#include "mpegheader.hpp"
#include "exceptions.hpp"
#include "bitreader.hpp"
#include "file.hpp"
#include "picture.hpp"

MPEGHeader *MPEGHeader::make( BitReader &hdr, File *file )
{
  ahabassert( hdr.readbits( 24 ) == 0x1 );

  uint8_t val = hdr.readbits( 8 );
  int extension_start_code_identifier;

  /* process slice_start_code */
  if ( (val >= 0x01) && (val <= 0xAF) ) {
    return new Slice( val, file );
  }

  /* process system start codes */
  if ( val >= 0xB9 ) {
    fprintf( stderr,
	     "Saw system start code (0x%02x). Does not appear to be an elementary stream.",
	     val );
    throw NotMPEGES();
  }

  switch ( val ) {
  case 0x00:
    return new Picture( hdr );
    break;

  case 0xB0:
  case 0xB1:
  case 0xB6:
    /* reserved */
    return new ReservedHeader( hdr );
    break;

  case 0xB2:
    /* user data */
    return new UserData( hdr );
    break;

  case 0xB3:
    return new Sequence( hdr );
    break;

  case 0xB4:
    /* sequence error */
    return new SequenceError( hdr );
    break;

  case 0xB5:
    extension_start_code_identifier = hdr.readbits( 4 );

    switch ( extension_start_code_identifier ) {
    case 1: return new SequenceExtension( hdr ); break;
    case 3: return new QuantMatrixExtension( hdr ); break;
    case 8: return new PictureCodingExtension( hdr ); break;
    default: return new OtherExtension( hdr ); break;
    }

    break;

  case 0xB7:
    /* sequence end */
    return new SequenceEnd( hdr );
    break;

  case 0xB8:
    /* group start */
    return new Group( hdr );
    break;

  default:
    throw InternalError();
  }

  ahabassert( 0 );
  return NULL;
}
