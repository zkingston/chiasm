#ifndef CHIASM_UTIL_
#define CHIASM_UTIL_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include <chiasm/types.h>

#define CH_STR2(s) #s
#define CH_STR(s) CH_STR2(s)

#define CH_OPTS "p:d:t:b:f:g:"

#define CH_DEFAULT_DEVICE    "/dev/video0"
#define CH_DEFAULT_FORMAT    "YUYV"
#define CH_DEFAULT_WIDTH     320
#define CH_DEFAULT_HEIGHT    240
#define CH_DEFAULT_BUFNUM    5
#define CH_DEFAULT_TIMEOUT   2.0
#define CH_DEFAULT_NUMFRAMES 0
#define CH_DEFAULT_OUTFMT    "RGB24"

#define CH_HELP_D \
    " -d   Device name. " CH_STR(CH_DEFAULT_DEVICE) " by default.\n"

#define CH_HELP_F \
    " -f   Image format code. " CH_STR(CH_DEFAULT_FORMAT) " by default.\n"

#define CH_HELP_G \
    " -g   Frame geometry in <w>x<h> format. " CH_STR(CH_DEFAULT_WIDTH) "x" CH_STR(CH_DEFAULT_HEIGHT) " by default.\n"

#define CH_HELP_B \
    " -b   Specify number of buffers to request. " CH_STR(CH_DEFAULT_BUFNUM) " by default.\n"

#define CH_HELP_T \
    " -t   Timeout in seconds. " CH_STR(CH_DEFAULT_TIMEOUT) " by default.\n"

#define CH_HELP_P \
    " -p   Output image format code. " CH_STR(CH_DEFAULT_OUTFMT) " by default.\n"

#define CH_CLEAR(x) (memset((x) , 0, sizeof(*(x))))

/**
 * @brief Robust wrapper around calloc.
 *
 * @param nmemb Number of elements to allocate.
 * @param size Size of each element.
 * @return Pointer to allocated memory on success, NULL on failure.
 */
void *ch_calloc(size_t nmemb, size_t size);

/**
 * @brief Converts a pixelformat character code to a null-terminated string.
 *
 * @param pixfmt Pixel format character code.
 * @param buf Buffer to fill in. Must be 5 elements long.
 * @return None.
 */
void ch_pixfmt_to_string(uint32_t pixfmt, char *buf);

/**
 * @brief Converts a string into a pixelformat character code.
 *
 * @param buf Buffer to convert.
 * @return Pixel format code from buffer.
 */
uint32_t ch_string_to_pixfmt(const char *buf);

/**
 * @brief Converts an amount in seconds to a struct timeval.
 *
 * @param seconds Time in seconds.
 * @return Time in a struct timeval.
 */
struct timeval ch_sec_to_timeval(double seconds);

/**
 * @brief Enables or disables logging in library functions.
 *
 * @param val Boolean on/off.
 * @return None.
 */
void ch_set_log(bool val);

/**
 * @brief Enables or disables logging in library functions output to stderr.
 *
 * @param val Boolean on/off.
 * @return None.
 */
void ch_set_stderr(bool val);

/**
 * @brief Error message with stringified error number and message.
 *
 * @param buf Error message.
 * @param err Error code.
 * @return None.
 */
void ch_error_no(const char *buf, int err);

/**
 * @brief Error message logging.
 *
 * @param buf Error message.
 * @return None.
 */
void ch_error(const char *buf);

#ifdef __cplusplus
}
#endif

#endif
