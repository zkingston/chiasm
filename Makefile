CC      = gcc
CFLAGS  = -Wall -Wextra -g3 -O3 -Iinclude `pkg-config --cflags gtk+-2.0` -fPIC
LDFLAGS = -ljpeg -lavutil -lavformat -lavcodec -lpthread -ldl

BINDIR  = bin
SRCDIR  = src

LIBSRCS = $(SRCDIR)/device.c $(SRCDIR)/decode.c $(SRCDIR)/util.c $(SRCDIR)/plugin.c
LIBOBJS = $(patsubst %.c, %.o, $(LIBSRCS))
LIBRARY = libchiasm.a

.PHONY: all
all: $(LIBRARY) display stream control output

$(LIBRARY): $(LIBOBJS)
	ar ru $@ $^
	ranlib $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

display: src/display.o $(LIBRARY)
	$(CC) -o $(BINDIR)/$@ $(CFLAGS) $< $(LIBRARY) $(LDFLAGS) \
	`pkg-config --libs gtk+-2.0 --libs gthread-2.0`

stream: src/stream.o $(LIBRARY)
	$(CC) -o $(BINDIR)/$@ $< $(LIBRARY) $(LDFLAGS)

control: src/control.o $(LIBRARY)
	$(CC) -o $(BINDIR)/$@ $< $(LIBRARY) $(LDFLAGS)

output: src/plugin/output.o
	$(CC) -shared -rdynamic -Wl,-export-dynamic,-soname,$(BINDIR)/$@.so \
	-o $(BINDIR)/$@.so $<

clean:
	rm $(LIBRARY)
	rm -r $(BINDIR)/*
	rm $(SRCDIR)/*.o
	rm $(SRCDIR)/plugin/*.o
