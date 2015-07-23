#ifndef CHIASM_TYPES_H_
#define CHIASM_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <setjmp.h>

#include <jpeglib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

struct ch_rect {
    uint32_t width;
    uint32_t height;
};

struct ch_fmts {
    uint32_t *fmts;
    uint32_t  length;
};

struct ch_frmsizes {
    struct ch_rect *frmsizes;
    uint32_t        length;
};

union ch_ctrl_menu_item {
    char    name[32];
    int64_t value;
};

struct ch_ctrl_menu {
    union ch_ctrl_menu_item *items;
    uint32_t length;
};

struct ch_ctrl {
    uint32_t id;
    char     name[32];
    uint32_t type;
    int32_t  min;
    int32_t  max;
    int32_t  step;
    int32_t  defval;
};

struct ch_ctrls {
    struct ch_ctrl *ctrls;
    uint32_t length;
};

struct ch_frmbuf {
    uint8_t  *start;
    uint32_t  length;
};

struct ch_jpeg_error_cx {
    struct jpeg_error_mgr pub;
    jmp_buf cx;
};

struct ch_decode_cx {
    AVCodecContext *codec_cx;
    AVFrame *frame_in;
    AVFrame *frame_out;
    bool compressed;
};

struct ch_device {
    char *name;
    int   fd;

    struct ch_frmbuf *in_buffers;
    uint32_t          num_buffers;

    struct ch_frmbuf *in_buffer;

    struct ch_frmbuf out_buffer;
    pthread_mutex_t  out_mutex;

    struct ch_rect framesize;
    uint32_t       pixelformat;

    struct timeval timeout;
    bool stream;

    pthread_t thread;

    struct ch_decode_cx decode_cx;
};

struct ch_stream_args {
    struct ch_device *device;
    uint32_t n_frames;
    int (*callback)(struct ch_device *device);
};

struct ch_dl {
    void *so;
    int (*init)(struct ch_device *device);
    int (*callback)(struct ch_device *device);
    int (*quit)(struct ch_device *device);
};

#ifdef __cplusplus
}
#endif

#endif
