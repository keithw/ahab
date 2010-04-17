#ifndef PICTURE_HPP
#define PICTURE_HPP

enum PictureType { I = 1, P, B };

#include "mpegheader.hpp"

class Frame;
class FrameHandle;
class DecodeSlices;
class DecodeEngine;

class Picture : public MPEGHeader
{
private:
  int coded_order, display_order;
  PictureType type;

  uint16_t temporal_reference;
  uint16_t vbv_delay;

  Sequence *sequence;
  PictureCodingExtension *extension;

  bool unclean_last_anchor;
  bool incomplete;
  bool broken;
  bool unknown_quantiser_matrix;
  bool invalid;

  double presentation_time;

  uint8_t *intra_quantiser_matrix,
    *non_intra_quantiser_matrix;

  Slice **first_slice_in_row;

  Picture *forward_reference, *backward_reference;

  void setup_decoder( mpeg2_decoder_t *d,
		      uint8_t *current_fbuf[3],
		      uint8_t *forward_fbuf[3],
		      uint8_t *backward_fbuf[3] );

  static void motion_setup( mpeg2_decoder_t *d );

  FrameHandle *fh;

  pthread_mutex_t decoding_mutex;
  pthread_cond_t decoding_activity;
  int decoding;

public:
  int get_coded( void ) { return coded_order; }
  void set_coded( int s_coded ) { coded_order = s_coded; }

  int get_display( void ) { return display_order; }
  void set_display( int s_display ) { display_order = s_display; }

  bool get_unclean( void ) { return unclean_last_anchor; }
  void set_unclean( bool s_unclean ) { unclean_last_anchor = s_unclean; }

  bool get_broken( void ) { return broken; }
  void set_broken( bool s ) { broken = s; }

  void set_unknown_quantiser_matrix( bool s_unknown ) { unknown_quantiser_matrix = s_unknown; }
  bool get_unknown_quantiser_matrix( void ) { return unknown_quantiser_matrix; }

  bool get_incomplete( void ) { return incomplete; }

  bool get_invalid( void ) { return invalid; }

  bool problem( void ) { return (get_broken() || get_unknown_quantiser_matrix() || get_incomplete() || get_invalid()); }

  double get_time( void ) { return presentation_time; }
  void set_time( double s_time ) { presentation_time = s_time; }

  void set_intra( uint8_t *s ) { intra_quantiser_matrix = s; }
  void set_non_intra( uint8_t *s ) { non_intra_quantiser_matrix = s; }

  uint num_fields( void );

  PictureType get_type( void ) { return type; }
  char get_type_char( void ) { return "XIPB"[type]; }

  void set_sequence( Sequence *s )
  {
    ahabassert( sequence == NULL );
    sequence = s;
  }

  Sequence *get_sequence( void ) {
    ahabassert( sequence );
    return sequence;
  }

  PictureCodingExtension *get_extension( void ) {
    ahabassert( extension );
    return extension;
  }

  void set_forward( Picture *s ) {
    ahabassert( forward_reference == NULL );
    ahabassert( (type == P) || (type == B) );
    forward_reference = s;
  }

  void set_backward( Picture *s ) {
    ahabassert( backward_reference == NULL );
    ahabassert( type == B );
    backward_reference = s;
  }

  Picture *get_forward( void ) { return forward_reference; }
  Picture *get_backward( void ) { return backward_reference; }

  Slice *get_first_slice_in_row( uint row ) { return first_slice_in_row[ row ]; }

  int get_f_code_fv( void ) { return get_extension()->f_code_fv; }
  int get_f_code_bv( void ) { return get_extension()->f_code_bv; }

  FrameHandle *get_framehandle( void ) { return fh; }

  Picture( BitReader &hdr );
  ~Picture();

  void init_fh( BufferPool *pool );

  virtual void print_info( void );
  virtual void link( void );

  void lock_and_decodeall();
  void start_parallel_decode( DecodeEngine *engine, bool leave_locked );
  void decoder_internal( DecodeSlices *job );
  void decoder_cleanup_internal( bool leave_locked );

};

#endif
