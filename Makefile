CC      = gcc
CFLAGS  = -Wall -Wextra -g3 -O3 -Iinclude `pkg-config --cflags gtk+-2.0`
LDFLAGS = -ljpeg -lavutil -lavformat -lavcodec -lpthread

BINDIR  = bin
SRCDIR  = src

LIBSRCS = $(SRCDIR)/device.c $(SRCDIR)/decode.c $(SRCDIR)/util.c
LIBOBJS = $(patsubst %.c, %.o, $(LIBSRCS))
LIBRARY = libchiasm.a

.PHONY: all
all: $(LIBRARY) display stream

$(LIBRARY): $(LIBOBJS)
	ar ru $@ $^
	ranlib $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

display: src/display.o $(LIBRARY)
	$(CC) -o $(BINDIR)/$@ $(CFLAGS) $< $(LIBRARY) $(LDFLAGS) \
	`pkg-config --libs gtk+-2.0 --libs gthread-2.0`

stream: src/stream.o $(LIBRARY)
	$(CC) -o $(BINDIR)/$@ $(CFLAGS) $< $(LIBRARY) $(LDFLAGS)

clean:
	rm $(LIBRARY)
	rm -r $(BINDIR)/*
	rm $(SRCDIR)/*.o
