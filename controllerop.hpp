#ifndef CONTROLLEROP_HPP
#define CONTROLLEROP_HPP

#include <pthread.h>

class ControllerState;

class ControllerOperation {
public:
  virtual void execute( ControllerState &state ) = 0;
  virtual ~ControllerOperation() {}
};

class MoveSlider : public ControllerOperation {
private:
  int picture_number;
  
public:
  MoveSlider( int s_picture_number ) : picture_number( s_picture_number ) {}
  ~MoveSlider() {}
  void execute( ControllerState &state );
};

#endif
