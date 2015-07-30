CC      = gcc
CXX     = g++
CFLAGS  = -Wall -Wextra -Iinclude -fPIC
LDFLAGS = -lswscale -lavutil -lavformat -lavcodec -lpthread -ldl

CFLAGS += -O2

# GTK configuration
CFLAGS  += `pkg-config --cflags gtk+-3.0`
LDFLAGS += `pkg-config --libs gtk+-3.0`

# C++ Stuff
CXXFLAGS = $(CFLAGS)

# AprilTag configuration. Requires C99 mode.
ATDIR   = ../apriltag
CFLAGS  += -I$(ATDIR) -std=gnu99
LDFLAGS += $(ATDIR)/libapriltag.a

# OpenCV configuration
CXXFLAGS  += `pkg-config --cflags opencv`
LDFLAGS += `pkg-config --libs opencv`

SRCDIR  = src
PLGDIR  = $(SRCDIR)/plugin

# Library configuration
LIBSRCS = $(SRCDIR)/device.c $(SRCDIR)/decode.c $(SRCDIR)/util.c $(SRCDIR)/plugin.c
LIBOBJS = $(patsubst %.c, %.o, $(LIBSRCS))

LIBSRCSX = $(SRCDIR)/distortion.cpp
LIBOBJSX = $(patsubst %.cpp, %.o, $(LIBSRCSX))

LIBOBJS += $(LIBOBJSX)

LIBRARY = libchiasm.a

# Output programs and plugins.
OBJS = stream control output.so display.so apriltag.so calibrate.so

.PHONY: all
all: $(LIBRARY) $(OBJS)

# libchiasm building.
$(LIBRARY): $(LIBOBJS)
	ar ru $@ $^
	ranlib $@
	rm $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Plugin building.
%.so: $(PLGDIR)/%.o
	$(CC) -shared -rdynamic -Wl,-export-dynamic,-soname,$(BINDIR)/$@ \
	-o $@ $< $(LIBRARY) $(LDFLAGS)

# Program building.
%: $(SRCDIR)/%.o $(LIBRARY)
	$(CXX) -o $@ $< $(LIBRARY) $(LDFLAGS)

# Clean up built objects.
clean:
	rm $(LIBRARY)
	rm $(OBJS)
