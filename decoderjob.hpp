#ifndef DECODERJOB_HPP
#define DECODERJOB_HPP

class Frame;

#include <stdlib.h>
#include <pthread.h>

#include "libmpeg2.h"
#include "exceptions.hpp"
#include "picture.hpp"

enum DecodeDirection { TOPDOWN, BOTTOMUP };

class DecoderJob
{
public:
  virtual void execute( void ) = 0;
  virtual ~DecoderJob() {}
};

class DecodeSlices : public DecoderJob
{
public:
  Picture *picture;
  DecodeDirection direction;
  mpeg2_decoder_t *decoder;
  Frame *cur, *fwd, *back;

  DecodeSlices( Picture *s_picture, DecodeDirection s_direction,
		mpeg2_decoder_t *s_decoder,
		Frame *s_cur, Frame *s_fwd, Frame *s_back )
    : picture( s_picture ), direction( s_direction ), decoder( s_decoder ),
      cur( s_cur ), fwd( s_fwd ), back( s_back )
  {}

  void execute( void ) {
    picture->decoder_internal( this );
  }

  ~DecodeSlices() {
    free( decoder );
  }
};

class Cleanup : public DecoderJob
{
private:
  Picture *picture;
  bool leave_locked;

public:
  Cleanup( Picture *s_picture, bool s_leave_locked )
    : picture( s_picture ), leave_locked( s_leave_locked )
  {}

  void execute( void ) {
    picture->decoder_cleanup_internal( leave_locked );
  }
};

class DecoderJobShutDown : public DecoderJob
{
public:
  void execute( void ) {
    pthread_exit( NULL );
  }
};

#endif
