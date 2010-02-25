#include <stdint.h>
#include <stdio.h>

#include "bitreader.hpp"
#include "exceptions.hpp"

inline bool BitReader::thisbit( void )
{
  uint octet = bit_offset / 8;
  uint offset_within_octet = 7 - (bit_offset % 8);

  return ( buf[ octet ] & (1 << offset_within_octet) ? true : false );
}

uint32_t BitReader::readbits( uint n )
{
  uint32_t val = 0;

  if ( (bit_offset + n - 1)/8 >= len ) {
    throw NeedBits();
  } 

  for ( uint i = 0; i < n; i++ ) {
    val <<= 1;
    val |= thisbit();
    bit_offset++;
  }

  return val;
}
