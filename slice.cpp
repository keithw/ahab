#include <errno.h>
#include <typeinfo>
#include <stdio.h>

#include "mpegheader.hpp"
#include "file.hpp"

Slice::Slice( uint s_val, File *s_file ) {
  init();

  val = s_val;
  file = s_file;

  next_slice_in_row = NULL;
  picture = NULL;
  len = 0;
  incomplete = false;
}

void Slice::link( void )
{
  MPEGHeader *hdr = get_next();
  if ( hdr == NULL ) {
    incomplete = true;
    return;
  }

  len = hdr->get_location() - get_location();
  if ( typeid( *hdr ) == typeid( Slice ) ) {
    Slice *ts = static_cast<Slice *>( hdr );
    if ( ts->get_val() == get_val() ) {
      next_slice_in_row = ts;
    }
  }
}

void Slice::print_info( void ) {
  printf( "s(%u,len=%u%s)", val, len, incomplete ? " [incomplete]" : "" );
}

MapHandle *Slice::map_chunk( void ) {
  return file->map( get_location(), len );
}
