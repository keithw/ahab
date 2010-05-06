#ifndef DECODEROP_HPP
#define DECODEROP_HPP

class DecoderOperation;
class DecoderState;

#include <X11/Xlib.h>
#include "decoder.hpp"

class DecoderOperation {
public:
  virtual void execute( DecoderState &state ) = 0;
  virtual ~DecoderOperation() {}
};

class SetPictureNumber : public DecoderOperation {
private:
  int picture_number;
  
public:
  SetPictureNumber( int s_picture_number ) : picture_number( s_picture_number ) {}
  ~SetPictureNumber() {}
  void execute( DecoderState &state ) { state.current_picture = picture_number; }
};

class XKey : public DecoderOperation {
private:
  KeySym key;

public:
  XKey( KeySym s_key ) : key( s_key ) {}
  ~XKey() {}
  void execute ( DecoderState &state );
};

class DecoderShutDown : public DecoderOperation {
public:
  DecoderShutDown() {}
  ~DecoderShutDown() {}
  void execute( DecoderState &state );
};

#endif
