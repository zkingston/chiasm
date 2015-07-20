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

#define CH_STR2(s) #s
#define CH_STR(s) CH_STR2(s)

#define CH_OPTS "d:t:b:f:g:"

#define CH_DEFAULT_DEVICE    "/dev/video0"
#define CH_DEFAULT_FORMAT    "YUYV"
#define CH_DEFAULT_WIDTH     320
#define CH_DEFAULT_HEIGHT    240
#define CH_DEFAULT_BUFNUM    5
#define CH_DEFAULT_TIMEOUT   2.0
#define CH_DEFAULT_NUMFRAMES 0

#define CH_HELP \
    " -d   Device name. " CH_STR(CH_DEFAULT_DEVICE) " by default.\n"	\
    " -f   Image format code. " CH_STR(CH_DEFAULT_FORMAT) " by default.\n" \
    " -g   Frame geometry in <w>x<h> format. " CH_STR(CH_DEFAULT_WIDTH) "x" CH_STR(CH_DEFAULT_HEIGHT) " by default.\n" \
    " -b   Specify number of buffers to request. " CH_STR(CH_DEFAULT_BUFNUM) " by default.\n" \
    " -t   Timeout in seconds. " CH_STR(CH_DEFAULT_TIMEOUT) " by default.\n"

#define CH_CLEAR(x) (memset((x) , 0, sizeof(*(x))))

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

    pthread_t thread;
};

struct ch_stream_args {
    struct ch_device *device;
    uint32_t n_frames;
    int (*callback)(struct ch_frmbuf *frm);
};

#include <chiasm/device.h>
#include <chiasm/decode.h>

#ifdef __cplusplus
}
#endif

#endif
