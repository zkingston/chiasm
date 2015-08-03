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

bin_PROGRAMS = ch_stream
ch_stream_SOURCES = src/stream.c
ch_stream_LDADD = libchiasm.la

bin_PROGRAMS += ch_control
ch_control_SOURCES = src/control.c
ch_control_LDADD = libchiasm.la

PLUGIN_LDFLAGS = -avoid-version -module -shared -export-dynamic libchiasm.la

pkglib_LTLIBRARIES = ch_output.la
ch_output_la_SOURCES = src/plugin/output.c
ch_output_la_LDFLAGS = $(PLUGIN_LDFLAGS)

pkglib_LTLIBRARIES += ch_display.la
ch_display_la_SOURCES = src/plugin/display.c
ch_display_la_CFLAGS = $(GTK_CFLAGS)
ch_display_la_LDFLAGS = $(PLUGIN_LDFLAGS) $(GTK_LIBS)

pkglib_LTLIBRARIES += ch_calibrate.la
ch_calibrate_la_SOURCES = src/plugin/calibrate.cpp
ch_calibrate_la_LDFLAGS = $(PLUGIN_LDFLAGS)

pkglib_LTLIBRARIES += ch_apriltag.la
ch_apriltag_la_SOURCES = src/plugin/apriltag.c
ch_apriltag_la_LDFLAGS = $(PLUGIN_LDFLAGS) -lapriltag
