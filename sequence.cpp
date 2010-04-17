#include <typeinfo>

#include "mpegheader.hpp"
#include "mpegtables.hpp"
#include "picture.hpp"

Sequence::Sequence( BitReader &hdr )
{
  init();

  hdr.reset();
  ahabassert ( hdr.readbits( 32 ) == 0x000001b3 );

  horizontal_size_value = hdr.readbits( 12 );
  vertical_size_value = hdr.readbits( 12 );

  uint atmp = hdr.readbits( 4 );
  switch ( atmp ) {
  case 1: aspect = SAR1x1; break;
  case 2: aspect = DAR4x3; break;
  case 3: aspect = DAR16x9; break;
  case 4: aspect = DAR221x100; break;
  default: throw MPEGInvalid(); break;
  }

  frame_rate_code = hdr.readbits( 4 );
  mpegassert( (frame_rate_code != 0) && (frame_rate_code <= 8) );

  bit_rate_value = hdr.readbits( 18 );
  mpegassert( hdr.readbits( 1 ) == 1 );
  vbv_buffer_size_value = hdr.readbits( 10 );
  constrained_parameters_flag = hdr.readbits( 1 );

  mpegassert( constrained_parameters_flag == 0 );

  uint load_intra_quantiser_matrix = hdr.readbits( 1 );
  for ( int i = 0; i < 64; i++ ) {
    intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] =
      load_intra_quantiser_matrix
      ? hdr.readbits( 8 )
      : default_intra_quantiser_matrix[ i ];
    mpegassert( intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] != 0 );
  }

  uint load_non_intra_quantiser_matrix = hdr.readbits( 1 );
  for ( int i = 0; i < 64; i++ ) {
    non_intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] =
      load_non_intra_quantiser_matrix ? hdr.readbits( 8 ) : 16;
    mpegassert( intra_quantiser_matrix[ mpeg2_normal_scan[ i ] ] != 0 );
  }
}

uint Sequence::get_horizontal_size( void )
{
  return (get_extension()->horizontal_size_extension << 12)
    | horizontal_size_value;
}

uint Sequence::get_vertical_size( void )
{
  return (get_extension()->vertical_size_extension << 12)
    | vertical_size_value;
}

uint Sequence::get_mb_width( void )
{
  return (get_horizontal_size() + 15) / 16;
}

uint Sequence::get_mb_height( void )
{
  if ( get_extension()->progressive_sequence ) {
    return (get_vertical_size() + 15)/16;
  } else {
    return 2*((get_vertical_size() + 31)/32);
  }
  /* We don't support field pictures. */
}

uint64_t Sequence::get_frame_rate_numerator( void )
{
  return sequence_numerators[ frame_rate_code ]
    * ( get_extension()->frame_rate_extension_n + 1 );
}

uint64_t Sequence::get_frame_rate_denominator( void )
{
  return sequence_denominators[ frame_rate_code ]
    * ( get_extension()->frame_rate_extension_d + 1 );
}

double Sequence::get_frame_rate( void )
{
  return (double)get_frame_rate_numerator() / (double)get_frame_rate_denominator();
}

void Sequence::set_unknown_quantiser_flags( void )
{
  MPEGHeader *hdr = get_next();

  while ( hdr && ( typeid( *hdr ) != typeid( Sequence ) ) ) {
    if ( typeid( *hdr ) == typeid( Picture ) ) {
      Picture *pic = static_cast<Picture *>( hdr );
      pic->set_unknown_quantiser_matrix( true );
    }

    hdr = hdr->get_next();
  }
}

void Sequence::link( void )
{
  /* We should handle this more gracefully if a stream is truncated
     after sequence header but before sequence extension XXX */

  /* Find my extension */
  SequenceExtension *se = dynamic_cast<SequenceExtension *>( get_next() );
  if ( se == NULL ) {
    fprintf( stderr, "Sequence extension not found at %lld.\n", get_location() );
    throw MPEGInvalid();
  }
  extension = se;

  if ( get_vertical_size() > 2800 ) {
    throw ConformanceLimitExceeded();
  }

  /* Set quantization matrices of pictures until next sequence */
  uint8_t *current_intra = intra_quantiser_matrix;
  uint8_t *current_non_intra = non_intra_quantiser_matrix;

  MPEGHeader *hdr = extension;
  while ( hdr && ( typeid( *hdr ) != typeid( Sequence ) ) ) {
    if ( typeid( *hdr ) == typeid( QuantMatrixExtension ) ) {
      QuantMatrixExtension *tq =
	static_cast<QuantMatrixExtension *>( hdr );
      uint8_t *new_intra = tq->get_intra_quantiser_matrix();
      uint8_t *new_non_intra = tq->get_non_intra_quantiser_matrix();

      if ( new_intra ) current_intra = new_intra;
      if ( new_non_intra ) current_non_intra = new_non_intra;
    } else if ( typeid( *hdr ) == typeid( Picture ) ) {
      Picture *tp = static_cast<Picture *>( hdr );
      tp->set_sequence( this );
      tp->set_intra( current_intra );
      tp->set_non_intra( current_non_intra );
    }

    hdr = hdr->get_next();
  }

  /* Make sure sequence parameters don't change */
  if ( hdr ) {
    Sequence *ts = dynamic_cast<Sequence *>( hdr );
    ahabassert( ts );
    mpegassert( horizontal_size_value == ts->horizontal_size_value );
    mpegassert( vertical_size_value == ts->vertical_size_value );
    //    mpegassert( aspect == ts->aspect );
    mpegassert( frame_rate_code == ts->frame_rate_code );
    //    mpegassert( bit_rate_value == ts->bit_rate_value );
    mpegassert( vbv_buffer_size_value == ts->vbv_buffer_size_value );
    mpegassert( constrained_parameters_flag == ts->constrained_parameters_flag );

    if ( (aspect != ts->aspect) || (bit_rate_value != ts->bit_rate_value) ) {
      fprintf( stderr, "Warning, sequence illegally changes parameters (aspect %d=>%d, bit_rate_value %d=>%d).\n",
	       aspect, ts->aspect, bit_rate_value, ts->bit_rate_value);
    }
  }
}

double Sequence::get_sar( void )
{
  double width = get_horizontal_size(); /* XXX should be display size if present*/
  double height = get_vertical_size();

  switch ( aspect ) {
  case SAR1x1: return 1;
  case DAR4x3: return (height/3.0) / (width/4.0);
  case DAR16x9: return (height/9.0) / (width/16.0);
  case DAR221x100: return (height/100.0) / (width/221.0);
  }

  throw AhabException();
}
