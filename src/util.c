#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>

#include <chiasm.h>

bool ch_log = true;
bool ch_stderr = false;
bool ch_log_enable = false;

inline void *
ch_calloc(size_t nmemb, size_t size)
{
    void *r = calloc(nmemb, size);

    // Check if we could not allocate memory.
    if (r == NULL || errno == ENOMEM) {
        ch_error("No memory available.");
        return (NULL);
    }

    return (r);
}

inline void
ch_pixfmt_to_string(uint32_t pixfmt, char *buf)
{
    size_t idx;
    for (idx = 0; idx < 4; idx++)
        buf[idx] = (pixfmt >> (8 * idx)) & 0xFF;

    buf[idx] = '\0';
}

inline uint32_t
ch_string_to_pixfmt(const char *buf)
{
    uint32_t pixfmt = 0;

    size_t idx;
    for (idx = 0; idx < 4 && buf[idx] != '\0'; idx++)
        pixfmt |= (buf[idx] << (8 * idx));

    return (pixfmt);
}

inline struct timeval
ch_sec_to_timeval(double seconds)
{
    struct timeval ret;
    ret.tv_sec = (long) seconds;
    ret.tv_usec = (long) ((seconds - (double) ret.tv_sec) * 1000000);

    return (ret);
}

inline void
ch_calc_stride(struct ch_device *device, uint32_t alignment)
{
    uint32_t stride = device->framesize.width;

    if ((stride % alignment) != 0)
        stride += alignment - (stride % alignment);

    device->out_stride = stride;
}

void
ch_set_log(bool val) {
    ch_log = val;
}

void
ch_set_stderr(bool val) {
    ch_stderr = val;
}

/**
 * @brief Enable logging if not already.
 *
 * @return None.
 */
static void
ch_enable_log(void) {
    openlog("libchiasm", LOG_PID | LOG_CONS, LOG_USER);
    ch_log_enable = true;
}

void
ch_error_no(const char *buf, int err) {
    if (!ch_log)
        return;

    if (!ch_log_enable)
        ch_enable_log();

    char err_buf[100];
    strerror_r(err, err_buf, 100);

    syslog(LOG_ERR, "%s [%d: %s]", buf, err, err_buf);

    if (ch_stderr)
        fprintf(stderr, "[CH_ERROR] %s [%d: %s]\n", buf, err, err_buf);
}

void
ch_error(const char *buf) {
    if (!ch_log)
        return;

    if (!ch_log_enable)
        ch_enable_log();

    syslog(LOG_ERR, "%s", buf);

    if (ch_stderr)
        fprintf(stderr, "[CH_ERROR] %s\n", buf);
}
