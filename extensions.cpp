#include "mpegheader.hpp"
#include "mpegtables.hpp"

SequenceExtension::SequenceExtension( BitReader &hdr )
{
  init();
  hdr.reset();

  ahabassert( hdr.readbits( 32 ) == 0x000001b5 );
  ahabassert( hdr.readbits( 4 ) == 1 );

  escape_bit = hdr.readbits( 1 );
  profile = hdr.readbits( 3 );
  level = hdr.readbits( 4 );
  progressive_sequence = hdr.readbits( 1 );
  chroma_format = hdr.readbits( 2 );
  horizontal_size_extension = hdr.readbits( 2 );
  vertical_size_extension = hdr.readbits( 2 );
  bit_rate_extension = hdr.readbits( 12 );

  mpegassert( hdr.readbits( 1 ) == 1 );
  
  vbv_buffer_size_extension = hdr.readbits( 8 );
  low_delay = hdr.readbits( 1 );
  frame_rate_extension_n = hdr.readbits( 2 );
  frame_rate_extension_d = hdr.readbits( 5 );

  /* Check conformance limits */
  if ( escape_bit ) {
    throw ConformanceLimitExceeded();
  }

  /* Simple profile or Main profile */
  if ( (profile != 5) && (profile != 4) ) {
    throw ConformanceLimitExceeded();
  }

  /* 4:2:0 only */
  if ( chroma_format != 1 ) {
    throw ConformanceLimitExceeded();
  }
}

bool SequenceExtension::operator==(const SequenceExtension &o) const {
  return ((escape_bit == o.escape_bit)
	  && (profile == o.profile)
	  && (level == o.level)
	  && (progressive_sequence == o.progressive_sequence)
	  && (chroma_format == o.chroma_format)
	  && (horizontal_size_extension == o.horizontal_size_extension)
	  && (vertical_size_extension == o.vertical_size_extension)
	  && (bit_rate_extension == o.bit_rate_extension)
	  && (vbv_buffer_size_extension == o.vbv_buffer_size_extension)
	  && (low_delay == o.low_delay)
	  && (frame_rate_extension_n == o.frame_rate_extension_n)
	  && (frame_rate_extension_d == o.frame_rate_extension_d));
}

void SequenceExtension::link( void )
{
  /* Make sure sequence paramenters don't change */
  MPEGHeader *hdr = get_next();
  while ( hdr ) {
    if ( SequenceExtension *ts = dynamic_cast<SequenceExtension *>( hdr ) ) {
      mpegassert( *this == *ts );
      break;
    } else {
      hdr = hdr->get_next();
    }
  }
}

QuantMatrixExtension::QuantMatrixExtension( BitReader &hdr )
{
  init();
  hdr.reset();

  ahabassert( hdr.readbits( 32 ) == 0x000001b5 );
  ahabassert( hdr.readbits( 4 ) == 3 );

  load_intra_quantiser_matrix = hdr.readbits( 1 );
  for ( int i = 0; i < 64; i++ ) {
    intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] =
      load_intra_quantiser_matrix
      ? hdr.readbits( 8 )
      : default_intra_quantiser_matrix[ i ];
    mpegassert( intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] != 0 );
  }

  load_non_intra_quantiser_matrix = hdr.readbits( 1 );
  for ( int i = 0; i < 64; i++ ) {
    non_intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] =
      load_non_intra_quantiser_matrix ? hdr.readbits( 8 ) : 16;
    mpegassert( intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] != 0 );
  }
}

PictureCodingExtension::PictureCodingExtension( BitReader &hdr )
{
  init();
  hdr.reset();

  ahabassert( hdr.readbits( 32 ) == 0x000001b5 );
  ahabassert( hdr.readbits( 4 ) == 8 );
  
  f_code_fh = hdr.readbits( 4 );
  f_code_fv = hdr.readbits( 4 );
  f_code_bh = hdr.readbits( 4 );
  f_code_bv = hdr.readbits( 4 );

  intra_dc_precision = hdr.readbits( 2 );
  picture_structure = hdr.readbits( 2 );

  top_field_first = hdr.readbits( 1 );
  frame_pred_frame_dct = hdr.readbits( 1 );
  concealment_motion_vectors = hdr.readbits( 1 );
  q_scale_type = hdr.readbits( 1 );
  intra_vlc_format = hdr.readbits( 1 );
  alternate_scan = hdr.readbits( 1 );
  repeat_first_field = hdr.readbits( 1 );
  chroma_420_type = hdr.readbits( 1 );
  progressive_frame = hdr.readbits( 1 );

  if ( picture_structure != 3 ) {
    fprintf( stderr, "Ahab does not support field pictures.\n" );
    throw ConformanceLimitExceeded();
  }

  mpegassert( ((f_code_fh >= 1) && (f_code_fh <= 9)) || (f_code_fh == 15) );
  mpegassert( ((f_code_fv >= 1) && (f_code_fv <= 9)) || (f_code_fv == 15) );
  mpegassert( ((f_code_bh >= 1) && (f_code_bh <= 9)) || (f_code_bh == 15) );
  mpegassert( ((f_code_bv >= 1) && (f_code_bv <= 9)) || (f_code_bv == 15) );
}
