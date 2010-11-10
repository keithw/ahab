#include "decoderjob.hpp"
#include "decoderop.hpp"
#include "displayop.hpp"
#include "framebuffer.hpp"
#include "decodeengine.hpp"
#include "controllerop.hpp"

template class Queue<DecoderJob>;
template class Queue<DecoderOperation>;
template class Queue<DisplayOperation>;
template class Queue<Frame>;
template class Queue<ReadyThread>;
template class Queue<ControllerOperation>;
