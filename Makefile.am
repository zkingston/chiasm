ACLOCAL_AMFLAGS = -Im4
AM_CPPFLAGS = -I$(top_srcdir)/include $(OPENCV_CFLAGS)

CHIASM_LIBS = $(OPENCV_LIBS) -lswscale -lavutil -lavformat -lavcodec -lpthread -ldl

lib_LTLIBRARIES = libchiasm.la
libchiasm_la_SOURCES = \
    src/decode.c \
    src/device.c \
    src/distortion.cpp \
    src/plugin.c \
    src/util.c
libchiasm_la_LIBADD = $(CHIASM_LIBS)

bin_PROGRAMS = stream
stream_SOURCES = src/stream.c
stream_LDADD = libchiasm.la

bin_PROGRAMS += control
control_SOURCES = src/control.c
control_LDADD = libchiasm.la

PLUGIN_LDFLAGS = -avoid-version -module -shared -export-dynamic libchiasm.la

pkglib_LTLIBRARIES = output.la
output_la_SOURCES = src/plugin/output.c
output_la_LDFLAGS = $(PLUGIN_LDFLAGS)

pkglib_LTLIBRARIES += display.la
display_la_SOURCES = src/plugin/display.c
display_la_CFLAGS = $(GTK_CFLAGS)
display_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(GTK_LIBS)

pkglib_LTLIBRARIES += calibrate.la
calibrate_la_SOURCES = src/plugin/calibrate.cpp
calibrate_la_LDFLAGS = $(PLUGIN_LDFLAGS)
