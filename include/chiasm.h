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

/**
 * @brief Converts a pixelformat character code to a null-terminated string.
 *
 * @param pixfmt Pixel format character code.
 * @param buf Buffer to fill in. Must be 5 elements long.
 * @return None.
 */
inline void ch_pixfmt_to_string(uint32_t pixfmt, char *buf);

/**
 * @brief Converts a string into a pixelformat character code.
 *
 * @param buf Buffer to convert.
 * @return Pixel format code from buffer.
 */
inline uint32_t ch_string_to_pixfmt(char *buf);

/**
 * @brief Converts an amount in seconds to a struct timeval.
 *
 * @param seconds Time in seconds.
 * @return Time in a struct timeval.
 */
inline struct timeval ch_sec_to_timeval(double seconds);

/**
 * @brief Initialize a struct ch_device.
 *
 * @param device The struct to fill.
 * @return None, fills in provided struct.
 */
void ch_init_device(struct ch_device *device);

/**
 * @brief Opens video device specified by name field in device.
 *
 * @param device The struct ch_device to open the device of.
 * @return 0 on success, -1 on failure. File-descriptor of device will be
 *         filled.
 */
int ch_open_device(struct ch_device *device);

/**
 * @brief Closes an opened video device.
 *
 * @param device Device to close and clean up.
 * @return 0 upon success, -1 upon failure.
 */
int ch_close_device(struct ch_device *device);

/**
 * @brief Return a list of all supported pixel formats for a device.
 *
 * @param device An opened device.
 * @return An allocated struct ch_fmts of all pixel formats available.
 */
struct ch_fmts *ch_enum_fmts(struct ch_device *device);

/**
 * @brief Deallocate a struct ch_fmts created by ch_enum_formats.
 *
 * @param fmts Allocated struct ch_fmts.
 * @return None.
 */
void ch_destroy_fmts(struct ch_fmts *fmts);

/**
 * @brief Return a list of all supported framesizes for a device's selected
 *        format.
 *
 * @param device An opened device.
 * @return An allocated struct ch_frmsizes of all framesizes available.
 */
struct ch_frmsizes *ch_enum_frmsizes(struct ch_device *device);

/**
 * @brief Deallocate a struct ch_frmsizes created by ch_enum_frmsizes.
 *
 * @param frmsizes Allocated struct ch_frmsizes.
 * @return None.
 */
void ch_destroy_frmsizes(struct ch_frmsizes *frmsizes);

/**
 * @brief Sets device format and framesize based on contents of provided struct.
 *
 * @param device Device to set with parameters.
 * @return 0 on success, -1 on failure.
 */
int ch_set_fmt(struct ch_device *device);

/**
 * @brief Initialize stream buffers.
 *
 * @param device Device to initialize.
 * @return 0 on success, -1 on failure.
 */
int ch_init_stream(struct ch_device *device);

/**
 * @brief Begin streaming from device.
 *
 * @param device Device to begin streaming from.
 * @return 0 on success, -1 on failure.
 */
int ch_start_stream(struct ch_device *device);

/**
 * @brief Stop streaming from a device.
 *
 * @param device Device to stop streaming from.
 * @return 0 on succes, -1 on failure.
 */
int ch_stop_stream(struct ch_device *device);

/**
 * @brief Stream video and call a callback upon every new frame.
 *
 * @param device Device to stream from.
 * @param num_frames Number of frames to stream. 0 for unlimited.
 * @param callback Function to callback on each new frame.
 */
int ch_stream(struct ch_device *device, uint32_t num_frames,
	      void (*callback)(struct ch_frmbuf *frm));

#endif
