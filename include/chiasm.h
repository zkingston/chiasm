#ifndef CHIASM_H_
#define CHIASM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#include <sys/time.h>

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

struct ch_device {
    char *name;
    int   fd;

    struct ch_frmbuf *in_buffers;
    uint32_t          num_buffers;

    struct ch_frmbuf out_buffer;
    pthread_mutex_t  out_mutex;

    struct ch_rect framesize;
    uint32_t       pixelformat;

    struct timeval timeout;
    bool stream;

    pthread_t thread;
};

struct ch_stream_args {
    struct ch_device *device;
    uint32_t n_frames;
    int (*callback)(struct ch_frmbuf *frm);
};

#include <chiasm/util.h>
#include <chiasm/device.h>
#include <chiasm/decode.h>

#ifdef __cplusplus
}
#endif

#endif
