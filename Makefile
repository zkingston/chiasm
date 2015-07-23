CC      = gcc
CFLAGS  = -Wall -Wextra -g3 -O3 -Iinclude `pkg-config --cflags gtk+-2.0` -fPIC
LDFLAGS = -lswscale -lavutil -lavformat -lavcodec -lpthread -ldl
LDFLAGS += `pkg-config --libs gtk+-2.0 --libs gthread-2.0`

SRCDIR  = src
PLGDIR  = $(SRCDIR)/plugin

LIBSRCS = $(SRCDIR)/device.c $(SRCDIR)/decode.c $(SRCDIR)/util.c $(SRCDIR)/plugin.c
LIBOBJS = $(patsubst %.c, %.o, $(LIBSRCS))
LIBRARY = libchiasm.a

OBJS = stream control output.so display.so

.PHONY: all
all: $(LIBRARY) $(OBJS)

$(LIBRARY): $(LIBOBJS)
	ar ru $@ $^
	ranlib $@
	rm $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.so: $(PLGDIR)/%.o
	$(CC) -shared -rdynamic -Wl,-export-dynamic,-soname,$(BINDIR)/$@ \
	-o $@ $< $(LIBRARY) $(LDFLAGS)

%: $(SRCDIR)/%.o $(LIBRARY)
	$(CC) -o $@ $< $(LIBRARY) $(LDFLAGS)

clean:
	rm $(LIBRARY)
	rm $(OBJS)
