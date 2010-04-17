#ifndef MPEGHEADER_HPP
#define MPEGHEADER_HPP

#include "libmpeg2.h"

#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

#include "bitreader.hpp"
#include "exceptions.hpp"
#include "file.hpp"

class ES;
class BufferPool;
class Picture;

class MPEGHeader {
private:
  off_t location;
  MPEGHeader *next;

protected:
  void init( void ) { location = -1; next = NULL; }
  
public:
  static MPEGHeader *make( BitReader &hdr, File *file );

  MPEGHeader *get_next( void ) { return next; }
  void set_next( MPEGHeader *s_next ) { ahabassert( next == NULL ); next = s_next; }
  void override_next( MPEGHeader *s_next ) { ahabassert( next != NULL ); next = s_next; }

  off_t get_location( void ) {
    ahabassert( location != -1 );
    return location;
  }
  void set_location( off_t s_location ) { ahabassert( location == -1 ); location = s_location; }

  virtual void print_info( void ) = 0;
  virtual void link( void ) = 0;
  virtual ~MPEGHeader( void ) {};
};

class SequenceExtension : public MPEGHeader
{
  friend class Sequence;

private:
  bool escape_bit;
  uint8_t profile;
  uint8_t level;
  bool progressive_sequence;
  uint8_t chroma_format;
  uint8_t horizontal_size_extension, vertical_size_extension;
  uint16_t bit_rate_extension;
  uint8_t vbv_buffer_size_extension;
  bool low_delay;
  uint8_t frame_rate_extension_n;
  uint8_t frame_rate_extension_d;

public:
  SequenceExtension( BitReader &hdr );
  virtual void print_info( void ) { printf( "sequence extension\n" ); }  
  virtual void link( void );
  bool operator==(const SequenceExtension &o) const;
};

class Sequence : public MPEGHeader
{
private:
  uint horizontal_size_value, vertical_size_value;
  enum AspectRatio { SAR1x1, DAR4x3, DAR16x9, DAR221x100 };
  AspectRatio aspect;
  uint8_t frame_rate_code;

  uint8_t intra_quantiser_matrix[ 64 ];
  uint8_t non_intra_quantiser_matrix[ 64 ];

  uint32_t bit_rate_value;
  uint16_t vbv_buffer_size_value;
  bool constrained_parameters_flag;

  SequenceExtension *extension;

public:
  virtual void print_info( void ) { printf( "sequence\n" ); }
  virtual void link( void );

  Sequence( BitReader &hdr );

  uint get_horizontal_size( void );
  uint get_vertical_size( void );
  uint get_mb_width( void );
  uint get_mb_height( void );
  uint64_t get_frame_rate_numerator( void );
  uint64_t get_frame_rate_denominator( void );
  double get_frame_rate( void );

  uint8_t *get_intra_quantiser_matrix( void ) {
    return intra_quantiser_matrix;
  }

  uint8_t *get_non_intra_quantiser_matrix( void ) {
    return non_intra_quantiser_matrix;
  }

  bool get_progressive_sequence( void ) { return get_extension()->progressive_sequence; }

  SequenceExtension *get_extension( void ) {
    ahabassert( extension );
    return extension;
  }

  AspectRatio get_aspect( void ) { return aspect; }
  double get_sar( void );
  void set_unknown_quantiser_flags( void );
};

class PictureCodingExtension : public MPEGHeader
{
  friend class Picture;

private:
  uint8_t f_code_fh, f_code_fv, f_code_bh, f_code_bv;
  uint8_t intra_dc_precision;
  uint8_t picture_structure;
  bool top_field_first, repeat_first_field, progressive_frame;
  bool frame_pred_frame_dct, concealment_motion_vectors,
    q_scale_type, intra_vlc_format, alternate_scan,
    chroma_420_type;

public:
  PictureCodingExtension( BitReader &hdr );
  virtual void print_info( void ) { printf( "picture coding extension\n" ); }
  virtual void link( void ) {}
};

class Picture;

class Slice : public MPEGHeader
{
private:
  uint val;
  Slice *next_slice_in_row;
  uint len;
  Picture *picture;
  bool incomplete;
  File *file;

public:
  Slice( uint s_val, File *s_file );
  virtual void link( void );

  uint get_len( void ) { return len; }
  uint get_val( void ) { return val; }
  uint top_line( void ) { return (val - 1) * 16; }
  uint bot_line( void ) { return val * 16 - 1; }
  virtual void print_info( void );
  Slice *get_next_in_row( void ) { return next_slice_in_row; }
  bool get_incomplete( void ) { return incomplete; }

  MapHandle *map_chunk( void );

  void set_picture( Picture *s ) { ahabassert( picture == NULL ); picture = s; }
  Picture *get_picture( void ) { ahabassert( picture ); return picture; }

  void decode( mpeg2_decoder_t * const decoder, const int code,
	       const uint8_t * const buffer);
};

class SequenceEnd : public MPEGHeader
{
public:
  SequenceEnd( BitReader &hdr ) { init(); }
  virtual void print_info( void ) { printf( "sequence end\n" ); }
  virtual void link( void ) {}
};

class OtherExtension : public MPEGHeader
{
public:
  OtherExtension( BitReader &hdr ) { init(); }
  virtual void print_info( void ) { printf( "other extension\n" ); }
  virtual void link( void ) {}
};

class ReservedHeader : public MPEGHeader
{
public:
  ReservedHeader( BitReader &hdr ) { init(); }
  virtual void print_info( void ) { printf( "reserved header\n" ); }
  virtual void link( void ) {}
};

class UserData : public MPEGHeader
{
public:
  UserData( BitReader &hdr ) { init(); }
  virtual void print_info( void ) { printf( "user data\n" ); }
  virtual void link( void ) {}
};

class SequenceError : public MPEGHeader
{
public:
  SequenceError( BitReader &hdr ) { init(); }
  virtual void print_info( void ) { printf( "sequence error\n" ); }
  virtual void link( void ) {}
};

class Group : public MPEGHeader
{
public:
  Group( BitReader &hdr ) { init(); }
  virtual void print_info( void ) { printf( "group\n" ); }
  virtual void link( void ) {}
};

class QuantMatrixExtension : public MPEGHeader
{
private:
  uint load_intra_quantiser_matrix;
  uint load_non_intra_quantiser_matrix;

  uint8_t intra_quantiser_matrix[ 64 ];
  uint8_t non_intra_quantiser_matrix[ 64 ];

public:
  uint8_t *get_intra_quantiser_matrix( void ) {
    return load_intra_quantiser_matrix ? intra_quantiser_matrix : NULL;
  }

  uint8_t *get_non_intra_quantiser_matrix( void ) {
    return load_non_intra_quantiser_matrix ? non_intra_quantiser_matrix : NULL;
  }  

  QuantMatrixExtension( BitReader &hdr );
  virtual void print_info( void ) { printf( "quant matrix extension\n" ); }
  virtual void link( void ) {}
};

#endif
