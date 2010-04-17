source = ahab.cpp es.cpp mpegheader.cpp bitreader.cpp startfinder.cpp sequence.cpp extensions.cpp exceptions.cpp picture.cpp slice.cpp file.cpp ogl.cpp slicedecode.cpp idct_mmx.cpp motion_comp_mmx.cpp framebuffer.cpp opq.cpp displayop.cpp controller.cpp displayopq.cpp decoder.cpp decoderop.cpp decoderopq.cpp xeventloop.cpp slicerow.cpp benchmark.cpp decodeengine.cpp readythreadq.cpp decoderjobq.cpp
objects = es.o mpegheader.o bitreader.o startfinder.o sequence.o extensions.o exceptions.o picture.o slice.o file.o ogl.o slicedecode.o idct_mmx.o motion_comp_mmx.o framebuffer.o opq.o displayop.o controller.o displayopq.o decoder.o decoderop.o decoderopq.o xeventloop.o slicerow.o decodeengine.o readythreadq.o decoderjobq.o
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
