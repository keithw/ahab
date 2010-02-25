#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <typeinfo>

#include "es.hpp"
#include "mpegheader.hpp"
#include "exceptions.hpp"
#include "bitreader.hpp"
#include "framebuffer.hpp"

const uint pool_slots = 50;

ES::ES( File *s_file, void (*progress)( off_t size, off_t location ) )
{
  file = s_file;
  first_header = last_header = NULL;

  /* Find first sequence header */
  /* We can't decode pictures before this, in general,
     because we don't know the quantization matrices. */
  off_t start = startfinder( 0, progress, &ES::first_sequence );

  if ( start == -1 ) {
    throw SequenceNotFound();
  }

  /* Ingest every start code, including before "start" */
  seq = NULL;
  startfinder( 0, progress, &ES::add_header );
  if ( !seq ) {
    throw SequenceNotFound();
  }

  /* Make ghost sequence header at start */
  MPEGHeader *real_first_header = first_header;

  Sequence *ghost_sequence = new Sequence( *seq );
  SequenceExtension *first_extension = dynamic_cast<SequenceExtension *>( seq->get_next() );
  SequenceExtension *ghost_extension = new SequenceExtension( *first_extension );

  if ( (ghost_sequence == NULL) || (first_extension == NULL) || (ghost_extension == NULL) ) {
    fprintf( stderr, "Problem assembling ghost sequence or extension header.\n" );
    throw MPEGInvalid();
  }

  first_header = ghost_sequence;
  ghost_sequence->override_next( ghost_extension );
  ghost_extension->override_next( real_first_header );

  ghost_sequence->set_unknown_quantiser_flags();

  /* Link headers to one another as appropriate */
  for ( MPEGHeader *hdr = first_header; hdr != NULL; hdr = hdr->get_next() ) {
      hdr->link();
  }

  pool = new BufferPool( pool_slots, 16 * seq->get_mb_width(),
			 16 * seq->get_mb_height() );

  /* Figure out the display order of each picture and link each
     from the coded_picture and displayed_picture arrays */
  number_pictures();

  /* Count up duration in seconds */
  duration_numer = 0;
  duration_denom = 2 * seq->get_frame_rate_numerator();
  uint ticks = seq->get_frame_rate_denominator();
  for ( uint i = 0; i < get_num_pictures(); i++ ) {
    displayed_picture[ i ]->set_time( duration_numer / (double)duration_denom );
    duration_numer += ticks * displayed_picture[ i ]->num_fields();
  }
  duration = duration_numer / (double)duration_denom;
}

ES::~ES()
{
  MPEGHeader *hdr = first_header;
  while ( hdr != NULL ) {
    MPEGHeader *next = hdr->get_next();
    delete hdr;
    hdr = next;
  }

  delete pool;
  delete[] coded_picture;
  delete[] displayed_picture;
}

void ES::number_pictures( void )
{
  /* Number each picture */
  Picture *oanchor = NULL;
  Picture *nanchor = NULL;
  uint coded_order = 0;
  uint display_order = 0;

  for ( MPEGHeader *hdr = first_header; hdr != NULL; hdr = hdr->get_next() ) {
    if ( typeid( *hdr ) == typeid( Picture ) ) {
      Picture *tp = static_cast<Picture *>( hdr );
      tp->set_coded( coded_order++ );

      switch ( tp->get_type() ) {
      case B:
	tp->set_display( display_order++ );
	tp->set_forward( oanchor );
	tp->set_backward( nanchor );
	if ( (oanchor == NULL) || (nanchor == NULL)
	     || (oanchor->problem()) || (nanchor->problem()) ) tp->set_broken( true );
	break;

      case P:
	tp->set_forward( nanchor );
	if ( (nanchor == NULL) || nanchor->problem() ) tp->set_broken( true );
	/* don't break */
      case I:
	if ( nanchor ) nanchor->set_display( display_order++ );
	oanchor = nanchor;
	nanchor = tp;
	break;
      }
    } else if ( typeid( *hdr ) == typeid( SequenceEnd ) ) {
      if ( nanchor ) nanchor->set_display( display_order++ );
      nanchor = NULL;
    }
  }

  if ( nanchor && (nanchor->get_display() == -1) ) {
    nanchor->set_display( display_order++ );
    nanchor->set_unclean( true );
    nanchor = NULL;
  }

  ahabassert( coded_order == display_order );
  num_pictures = coded_order;

  /* Allocate memory */
  coded_picture = new Picture *[ num_pictures ];
  displayed_picture = new Picture *[ num_pictures ];

  /* Make pointers in order */
  for ( MPEGHeader *hdr = first_header; hdr != NULL; hdr = hdr->get_next() ) {
    if ( typeid( *hdr ) == typeid( Picture ) ) {
      Picture *tp = static_cast<Picture *>( hdr );
      uint this_coded = tp->get_coded();
      uint this_display = tp->get_display();

      tp->init_fh( pool );

      ahabassert( (this_coded >= 0) && (this_coded < num_pictures) );
      ahabassert( (this_display >= 0) && (this_coded < num_pictures) );

      coded_picture[ this_coded ] = tp;
      displayed_picture[ this_display ] = tp;
    }
  }
}
