#ifndef BITREADER_HPP
#define BITREADER_HPP

#include <stdint.h>
#include <stdlib.h>

class BitReader {
private:
  uint8_t *buf;
  uint len;

  uint bit_offset;

  bool thisbit( void );

public:
  BitReader( uint8_t *s_buf, uint s_len ) {
    buf = s_buf;
    bit_offset = 0;
    len = s_len;
  }

  uint32_t readbits( uint n );
  void reset( void ) { bit_offset = 0; }
};

#endif
