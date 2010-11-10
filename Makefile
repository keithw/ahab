source = ahab.cpp benchmark.cpp bitreader.cpp controller.cpp decodeengine.cpp decoder.cpp decoderop.cpp displayop.cpp es.cpp exceptions.cpp extensions.cpp file.cpp framebuffer.cpp idct_mmx.cpp motion_comp_mmx.cpp mpegheader.cpp ogl.cpp opq.cpp picture.cpp queue_templates.cpp sequence.cpp slice.cpp slicedecode.cpp slicerow.cpp startfinder.cpp xeventloop.cpp controllerop.cpp
objects = bitreader.o controller.o decodeengine.o decoder.o decoderop.o displayop.o es.o exceptions.o extensions.o file.o framebuffer.o idct_mmx.o motion_comp_mmx.o mpegheader.o ogl.o opq.o picture.o queue_templates.o sequence.o slice.o slicedecode.o slicerow.o startfinder.o xeventloop.o controllerop.o
executables = ahab benchmark

CPP = g++
CPPFLAGS = -g -O3 -Wall -fno-implicit-templates -pipe -pthread -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE=500 -DGL_GLEXT_PROTOTYPES -DGLX_GLXEXT_PROTOTYPES `pkg-config gtkmm-2.4 --cflags`
LIBS = -lX11 -lGL -lGLU `pkg-config gtkmm-2.4 --libs`

all: $(executables)

controller.o: controller.cpp
	$(CPP) $(CPPFLAGS) -frepo -c -o $@ $<

ahab: ahab.o $(objects)
	$(CPP) $(CPPFLAGS) -o $@ $+ $(LIBS)

benchmark: benchmark.o $(objects)
	$(CPP) $(CPPFLAGS) -o $@ $+ $(LIBS) -lrt

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

include depend

depend: $(source)
	$(CPP) $(INCLUDES) -MM $(source) > depend

.PHONY: clean
clean:
	-rm -f $(executables) depend *.o *.rpo
