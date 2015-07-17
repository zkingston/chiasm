#ifndef CHIASM_H_
#define CHIASM_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#include <sys/time.h>

#define CH_DEFAULT_DEVICE    "/dev/video0"
#define CH_DEFAULT_FORMAT    "YUYV"
#define CH_DEFAULT_WIDTH     320
#define CH_DEFAULT_HEIGHT    240
#define CH_DEFAULT_BUFNUM    5
#define CH_DEFAULT_TIMEOUT   2.0
#define CH_DEFAULT_NUMFRAMES 0

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
};

#include <chiasm/device.h>
#include <chiasm/decode.h>

#endif
