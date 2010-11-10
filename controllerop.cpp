#include "controller.hpp"
#include "controllerop.hpp"

void MoveSlider::execute( ControllerState &state )
{
  state.scale->set_value( picture_number );

  /* Gtk+ doesn't redraw the number (value) unless the position
     of the slider changes or it gets exposed! This looks like
     a bug since we can even get partial numerals! */

  state.scale->hide();
  state.scale->show();
}
