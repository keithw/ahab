#include <string.h>
#include <typeinfo>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>

#include "libmpeg2.h"

#include "decoderjob.hpp"
#include "decodeengine.hpp"

#include "picture.hpp"

#include "mpegheader.hpp"
#include "framebuffer.hpp"
#include "exceptions.hpp"
#include "mutexobj.hpp"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

extern const uint8_t mpeg2_scan_norm[ 64 ]; /* These are the MMX versions */
extern const uint8_t mpeg2_scan_alt[ 64 ];

Picture::Picture( BitReader &hdr, File *s_file ) {
  init();
  file = s_file;
  sequence = NULL;
  extension = NULL;
  first_slice_in_row = NULL;
  coded_order = display_order = -1;
  set_unclean( false );
  set_broken( false );
  set_unknown_quantiser_matrix( false );
  incomplete = false;
  forward_reference = backward_reference = NULL;
  fh = NULL;
  decoding = 0;
  invalid = false;
  slices_start = slices_end = NULL;

  unixassert( pthread_mutex_init( &decoding_mutex, NULL ) );
  unixassert( pthread_cond_init( &decoding_activity, NULL ) );

  hdr.reset();
  ahabassert( hdr.readbits( 32 ) == 0x00000100 );

  temporal_reference = hdr.readbits( 10 );
  uint8_t picture_coding_type = hdr.readbits( 3 );
  switch ( picture_coding_type ) {
  case 1: type = I; break;
  case 2: type = P; break;
  case 3: type = B; break;
  default: throw MPEGInvalid(); break;
  }

  vbv_delay = hdr.readbits( 16 );
}

Picture::~Picture()
{
  if ( first_slice_in_row ) {
    delete[] first_slice_in_row;
  }
  if ( fh ) {
    delete fh;
  }

  unixassert( pthread_cond_destroy( &decoding_activity ) );
  unixassert( pthread_mutex_destroy( &decoding_mutex ) );
}

static const int non_linear_scale [] = {
  0,  1,  2,  3,  4,  5,   6,   7,
  8, 10, 12, 14, 16, 18,  20,  22,
  24, 28, 32, 36, 40, 44,  48,  52,
  56, 64, 72, 80, 88, 96, 104, 112
};

void Picture::link( void )
{
  /* We should handle this more gracefully if a stream is truncated
     after picture but before picture coding extension XXX */

  /* Find my extension */
  PictureCodingExtension *pe = dynamic_cast<PictureCodingExtension *>( get_next() );
  if ( pe == NULL ) {
    fprintf( stderr, "Picture coding extension not found at %ld.\n", (long)get_location() );
    throw MPEGInvalid();
  }
  extension = pe;

  /* Allocate space for first_slice_in_rows */
  uint mb_height = get_sequence()->get_mb_height();
  first_slice_in_row = new Slice *[ mb_height ];

  for ( uint i = 0; i < mb_height; i++ ) {
    first_slice_in_row[ i ] = NULL;
  }

  /* Collect all the first_slice_in_rows */
  MPEGHeader *hdr = extension;
  while ( hdr && ( typeid( *hdr ) != typeid( Picture ) ) ) {
    if ( typeid( *hdr ) == typeid( Slice ) ) {
      Slice *ts = static_cast<Slice *>( hdr );
      ts->set_picture( this );
      uint val = ts->get_val();
      mpegassert( (val > 0) && (val <= mb_height) );
      int loc = val - 1;
      if ( first_slice_in_row[ loc ] == NULL ) {
	first_slice_in_row[ loc ] = ts;
      }
    }

    hdr = hdr->get_next();
  }

  for ( uint i = 0; i < mb_height; i++ ) {
    if ( first_slice_in_row[ i ] == NULL ) {
      incomplete = true;
      break;
    }
  }
}

uint Picture::num_fields( void )
{
  PictureCodingExtension *ext = get_extension();

  uint fields = 0;

  /* Calculate how many fields picture is */
  if ( ext->picture_structure != 3 ) {
    fields = 1;
  } else {
    if ( get_sequence()->get_progressive_sequence() ) {
      if ( ext->repeat_first_field == 0 ) fields = 2;
      if ( (ext->repeat_first_field == 1) && (ext->top_field_first == 0) ) fields = 4;
      if ( (ext->repeat_first_field == 1) && (ext->top_field_first == 1) ) fields = 6;
    } else { /* interlaced sequence */
      if ( ext->progressive_frame == 0 ) fields = 2;
      if ( (ext->progressive_frame == 1) && (ext->repeat_first_field == 0) ) fields = 2;
      if ( (ext->progressive_frame == 1) && (ext->repeat_first_field == 1) ) fields = 3;
    }
  }

  mpegassert( fields != 0 );
  return fields;
}

void Picture::print_info( void ) {
  Picture *forw = get_forward();
  Picture *bakw = get_backward();
  
  char forstr[ 64 ], bakstr[ 64 ];

  if ( forw ) {
    snprintf( forstr, 64, "%d", forw->get_display() );
  } else {
    strcpy( forstr, "XXX" );
  }

  if ( bakw ) {
    snprintf( bakstr, 64, "%d", bakw->get_display() );
  } else {
    strcpy( bakstr, "XXX" );
  }

  char dependencies[64];
  switch ( type ) {
  case I: strcpy( dependencies, "" ); break;
  case P: snprintf( dependencies, 64, "(%s=>) ", forstr ); break;
  case B: snprintf( dependencies, 64, "(%s=>,=>%s) ", forstr, bakstr ); break;
  }

  PictureCodingExtension *e = get_extension();
  printf( "PICTURE[%c] %d %sis coded %d %s %s %s %s\n",
	  get_type_char(), get_display(), dependencies, get_coded(), get_unclean() ? "(unflushed anchor)" : "",
	  get_incomplete() ? "(missing slices)" : "",
	  get_broken() ? "(broken dependencies)" : "",
	  get_unknown_quantiser_matrix() ? "(unknown quantiser matrix)" : "" );
  printf( "params: intra_dc_precision=%d, picture_structure=%d, frame_pred_frame_dct=%d, concealment_motion_vectors=%d, q_scale_type=%d, intra_vlc_format=%d, alternate_scan=%d, top_field_first=%d\n", e->intra_dc_precision, e->picture_structure, e->frame_pred_frame_dct, e->concealment_motion_vectors, e->q_scale_type, e->intra_vlc_format, e->alternate_scan, e->top_field_first );
  printf( "intra_quantiser_matrix[ 64 ] = { " );
  for ( int i = 0; i < 64; i++ ) {
    printf( "%d, ", intra_quantiser_matrix[ i ] );
  }
  printf( "};\n non_intra_quantiser_matrix[ 64 ] = { " );
  for ( int i = 0; i < 64; i++ ) {
    printf( "%d, ", non_intra_quantiser_matrix[ i ] );
  }
  printf( "};\n" );
}

void Picture::setup_decoder( mpeg2_decoder_t *d, uint8_t *current_fbuf[3],
			     uint8_t *forward_fbuf[3],
			     uint8_t *backward_fbuf[3] )
{
  d->picture_structure = get_extension()->picture_structure;
  d->stride_frame = 16 * get_sequence()->get_mb_width();
  d->width = 16 * get_sequence()->get_mb_width();
  d->height = 16 * get_sequence()->get_mb_height();
  d->coding_type = type;
  d->vertical_position_extension = 0;
  d->chroma_format = 0; /* 4:2:0 */
  d->concealment_motion_vectors = get_extension()->concealment_motion_vectors;
  d->scan = get_extension()->alternate_scan ? mpeg2_scan_alt : mpeg2_scan_norm;
  d->intra_dc_precision = 7 - get_extension()->intra_dc_precision;
  d->frame_pred_frame_dct = get_extension()->frame_pred_frame_dct;
  d->q_scale_type = get_extension()->q_scale_type;
  d->intra_vlc_format = get_extension()->intra_vlc_format;
  d->top_field_first = get_extension()->top_field_first;
  d->convert = NULL;
  d->convert_id = NULL;
  d->mpeg1 = 0;
  
  d->f_motion.f_code[ 0 ] = get_extension()->f_code_fh - 1;
  d->f_motion.f_code[ 1 ] = get_extension()->f_code_fv - 1;
  d->b_motion.f_code[ 0 ] = get_extension()->f_code_bh - 1;
  d->b_motion.f_code[ 1 ] = get_extension()->f_code_bv - 1;

  /* Calculate prescaled quantisers */
  for ( uint i = 0; i < 32; i++ ) {
    int k = get_extension()->q_scale_type ? non_linear_scale[ i ] : (i << 1);
    for ( uint j = 0; j < 64; j++ ) {
      d->quantizer_prescale[ 0 ][ i ][ j ] =
        k * intra_quantiser_matrix[ j ];
      d->quantizer_prescale[ 1 ][ i ][ j ] =
        k * non_intra_quantiser_matrix[ j ];
    }
  }

  d->chroma_quantizer[ 0 ] = d->quantizer_prescale[ 0 ];
  d->chroma_quantizer[ 1 ] = d->quantizer_prescale[ 1 ];

  int stride, height;
  
  stride = d->stride_frame;
  height = d->height;
  
  d->picture_dest[0] = current_fbuf[0];
  d->picture_dest[1] = current_fbuf[1];
  d->picture_dest[2] = current_fbuf[2];
  
  d->f_motion.ref[0][0] = forward_fbuf[0];
  d->f_motion.ref[0][1] = forward_fbuf[1];
  d->f_motion.ref[0][2] = forward_fbuf[2];
  
  d->b_motion.ref[0][0] = backward_fbuf[0];
  d->b_motion.ref[0][1] = backward_fbuf[1];
  d->b_motion.ref[0][2] = backward_fbuf[2];
  
  d->stride = stride;
  d->uv_stride = stride >> 1;
  d->slice_stride = 16 * stride;
  d->slice_uv_stride =
    d->slice_stride >> (2 - d->chroma_format);
  d->limit_x = 2 * d->width - 32;
  d->limit_y_16 = 2 * height - 32;
  d->limit_y_8 = 2 * height - 16;
  d->limit_y = height - 16;

  d->invalid = false;

  memset( d->DCTblock, 0, 64 * sizeof( int16_t ) );

  motion_setup( d );
}

void Picture::start_parallel_decode( DecodeEngine *engine, bool leave_locked )
{
  /* First, see if the picture is already rendered */
  if ( fh->increment_lockcount_if_renderable() ) {
    if ( !leave_locked ) {
      fh->decrement_lockcount();
    }
    return;
  }

  /* If already in progress, let first thread handle. Otherwise mark our turf. */
  {
    MutexLock x( &decoding_mutex );
    if ( decoding > 0 ) {
      if ( leave_locked ) {
	fh->increment_lockcount();
      }
      return;
    } else {
      decoding++;
    }
  }

  /* Lock and start decoding pre-requisites */
  if ( forward_reference ) {
    forward_reference->start_parallel_decode( engine, true );
  }

  if ( backward_reference ) {
    backward_reference->start_parallel_decode( engine, true );
  }

  /* Lock myself */
  fh->increment_lockcount();

  Frame *cur, *fwd, *back;

  cur = fwd = back = fh->get_frame();

  if ( forward_reference ) fwd = forward_reference->get_framehandle()->get_frame();
  if ( backward_reference ) back = backward_reference->get_framehandle()->get_frame();

  uint8_t *curf[3] = { cur->get_y(), cur->get_cb(), cur->get_cr() };
  uint8_t *fwdf[3] = { fwd->get_y(), fwd->get_cb(), fwd->get_cr() };
  uint8_t *backf[3] = { back->get_y(), back->get_cb(), back->get_cr() };

  uint height = 16 * get_sequence()->get_mb_height();
  uint width = 16 * get_sequence()->get_mb_width();

  if ( problem() ) {
    memset( curf[0], 128, 3 * height * width / 2 );
  }

  mpeg2_decoder_t *topdown_d, *bottomup_d;
  unixassert( posix_memalign( (void **)&topdown_d, 64, sizeof( mpeg2_decoder_t ) ) );
  unixassert( posix_memalign( (void **)&bottomup_d, 64, sizeof( mpeg2_decoder_t ) ) );

  setup_decoder( topdown_d, curf, fwdf, backf );
  setup_decoder( bottomup_d, curf, fwdf, backf );

  DecodeSlices *topdown = new DecodeSlices( this, TOPDOWN, topdown_d, cur, fwd, back );
  DecodeSlices *bottomup = new DecodeSlices( this, BOTTOMUP, bottomup_d, cur, fwd, back );
  Cleanup *cleanup = new Cleanup( this, leave_locked );

  {
    MutexLock x( &decoding_mutex );
    decoding += 2;
  }

  engine->dispatch( topdown );
  engine->dispatch( bottomup );

  engine->dispatch( cleanup );
}

void Picture::decoder_cleanup_internal( bool leave_locked )
{
  {
    MutexLock x( &decoding_mutex );
    ahabassert( decoding > 0 );

    while ( decoding != 1 ) {
      unixassert( pthread_cond_wait( &decoding_activity, &decoding_mutex ) );
    }
  }

  if ( forward_reference ) {
    forward_reference->get_framehandle()->wait_rendered();
    forward_reference->get_framehandle()->decrement_lockcount();  
  }
  if ( backward_reference ) {
    backward_reference->get_framehandle()->wait_rendered();
    backward_reference->get_framehandle()->decrement_lockcount();
  }

  fh->get_frame()->set_rendered();

  if ( !leave_locked ) {
    fh->decrement_lockcount();
  }

  {
    MutexLock x( &decoding_mutex );
    decoding = 0;
  }
}

void Picture::decoder_internal( DecodeSlices *job )
{
  int rows = get_sequence()->get_mb_height();

  int starting_row, increment;

  if ( job->direction == TOPDOWN ) {
    starting_row = 0;
    increment = 1;
  } else {
    starting_row = rows - 1;
    increment = -1;
  }

  int row = starting_row;
  SliceRow *first_sr = job->cur->get_slicerow( row );
  if ( forward_reference ) {
    int fw_bot = first_sr->get_forward_lowrow();
    int fw_top = first_sr->get_forward_highrow();

    if ( fw_bot != -1 ) {
      for ( int depend_row = fw_top; depend_row <= fw_bot; depend_row++ ) {
	job->fwd->get_slicerow( depend_row )->wait_rendered();
      }
    }
  }

  if ( backward_reference ) {
    int back_bot = first_sr->get_backward_lowrow();
    int back_top = first_sr->get_backward_highrow();

    if ( back_bot != -1 ) {
      for ( int depend_row = back_top; depend_row <= back_bot; depend_row++ ) {
	job->back->get_slicerow( depend_row )->wait_rendered();
      }
    }
  }

  MapHandle *chunk = file->map( slices_start, slices_end - slices_start );

  while ( 0 <= row && row < rows ) {
    SliceRow *sr = job->cur->get_slicerow( row );
    SliceRowState previous_state = sr->lock();
    if ( (previous_state == SR_LOCKED) || (previous_state == SR_RENDERED) ) {
      break;
    }

    if ( forward_reference ) {
      int depend_row;
      if ( job->direction == TOPDOWN ) {
	depend_row = sr->get_forward_lowrow();
      } else {
	depend_row = sr->get_forward_highrow();
      }

      if ( depend_row != -1 ) {
	job->fwd->get_slicerow( depend_row )->wait_rendered();
      }
    }

    if ( backward_reference ) {
      int depend_row;
      if ( job->direction == TOPDOWN ) {
	depend_row = sr->get_backward_lowrow();
      } else {
	depend_row = sr->get_backward_highrow();
      }

      if ( depend_row != -1 ) {
	job->back->get_slicerow( depend_row )->wait_rendered();
      }
    }

    Slice *s = get_first_slice_in_row( row );
    while ( s != NULL ) {
      off_t slice_offset = s->get_location() - slices_start;

      job->decoder->bitstream_buf = 0;
      job->decoder->bitstream_bits = 0;
      job->decoder->bitstream_ptr = chunk->get_buf() + slice_offset + 4;
      job->decoder->bit_ptr_end = chunk->get_buf() + slice_offset + s->get_len();

      s->decode( job->decoder, s->get_val(), chunk->get_buf() + slice_offset + 4 );

      if ( job->decoder->invalid ) {
	invalid = true; /* XXX should be protected by mutex */
	job->decoder->invalid = false;
      }

      s = s->get_next_in_row();
    }

    sr->set_rendered();
    row += increment;
  }

  delete chunk;

  {
    MutexLock x( &decoding_mutex );
    decoding--;
    unixassert( pthread_cond_broadcast( &decoding_activity ) );
  }
}

void Picture::lock_and_decodeall( void )
{
  fh->increment_lockcount();

  if ( fh->get_frame()
       && fh->get_frame()->get_state() == RENDERED ) {
    return;
  }

  fh->decrement_lockcount();

  Frame *cur, *fwd, *back;

  /* Lock and decode pre-requisites */
  if ( forward_reference ) {
    forward_reference->lock_and_decodeall();
  }

  if ( backward_reference ) {
    backward_reference->lock_and_decodeall();
  }

  /* Lock myself */
  fh->increment_lockcount();

  cur = fwd = back = fh->get_frame();

  if ( forward_reference ) fwd = forward_reference->get_framehandle()->get_frame();
  if ( backward_reference ) back = backward_reference->get_framehandle()->get_frame();

  uint8_t *curf[3] = { cur->get_y(), cur->get_cb(), cur->get_cr() };
  uint8_t *fwdf[3] = { fwd->get_y(), fwd->get_cb(), fwd->get_cr() };
  uint8_t *backf[3] = { back->get_y(), back->get_cb(), back->get_cr() };

  uint height = 16 * get_sequence()->get_mb_height();
  uint width = 16 * get_sequence()->get_mb_width();

  if ( problem() ) {
    memset( cur->get_y(), 128, height * width );
    memset( cur->get_cb(), 128, height * width / 4 );
    memset( cur->get_cr(), 128, height * width / 4 );
  }

  uint rows = get_sequence()->get_mb_height();

  mpeg2_decoder_t d;
  setup_decoder( &d, curf, fwdf, backf );

  for ( uint row = 0; row < rows; row++ ) {
    Slice *s = get_first_slice_in_row( row );
    while ( s != NULL ) {
      MapHandle *chunk = s->map_chunk();

      d.bitstream_buf = 0;
      d.bitstream_bits = 0;
      d.bitstream_ptr = chunk->get_buf() + 4;
      d.bit_ptr_end = chunk->get_buf() + chunk->get_len();

      s->decode( &d, s->get_val(), chunk->get_buf() + 4 );

      if ( d.invalid ) {
	invalid = true;
	d.invalid = false;
      }

      delete chunk;

      s = s->get_next_in_row();
    }
  }

  fh->get_frame()->set_rendered();

  if ( forward_reference ) forward_reference->get_framehandle()->decrement_lockcount();  
  if ( backward_reference ) backward_reference->get_framehandle()->decrement_lockcount();

  /* leave myself locked */
}

void Picture::init_fh( BufferPool *pool )
{
  fh = pool->make_handle( this );
}

void Picture::register_slice_extent( off_t start, off_t end )
{
  if ( !slices_start ) {
    slices_start = start;
    slices_end = end;
    return;
  }

  if ( start < slices_start ) {
    slices_start = start;
  }

  if ( end > slices_end ) {
    slices_end = end;
  }
}
