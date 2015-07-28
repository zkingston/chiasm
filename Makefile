CC      = gcc
CFLAGS  = -Wall -Wextra -g3 -O3 -Iinclude -fPIC
LDFLAGS = -lswscale -lavutil -lavformat -lavcodec -lpthread -ldl

# GTK configuration
CFLAGS  += `pkg-config --cflags gtk+-3.0`
LDFLAGS += `pkg-config --libs gtk+-3.0`

# AprilTag configuration. Requires C99 mode.
ATDIR   = ../apriltag
CFLAGS  += -I$(ATDIR) -std=gnu99
LDFLAGS += $(ATDIR)/libapriltag.a

SRCDIR  = src
PLGDIR  = $(SRCDIR)/plugin

# Library configuration
LIBSRCS = $(SRCDIR)/device.c $(SRCDIR)/decode.c $(SRCDIR)/util.c $(SRCDIR)/plugin.c
LIBOBJS = $(patsubst %.c, %.o, $(LIBSRCS))
LIBRARY = libchiasm.a

# Output programs and plugins.
OBJS = stream control output.so # display.so apriltag.so

.PHONY: all
all: $(LIBRARY) $(OBJS)

# libchiasm building.
$(LIBRARY): $(LIBOBJS)
	ar ru $@ $^
	ranlib $@
	rm $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Plugin building.
%.so: $(PLGDIR)/%.o
	$(CC) -shared -rdynamic -Wl,-export-dynamic,-soname,$(BINDIR)/$@ \
	-o $@ $< $(LIBRARY) $(LDFLAGS)

# Program building.
%: $(SRCDIR)/%.o $(LIBRARY)
	$(CC) -o $@ $< $(LIBRARY) $(LDFLAGS)

# Clean up built objects.
clean:
	rm $(LIBRARY)
	rm $(OBJS)
