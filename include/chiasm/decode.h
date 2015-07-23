#ifndef CHIASM_DECODE_H_
#define CHIASM_DECODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>

// Backwards compatibility.
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc  avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

/**
 * @brief Convert an YUYV format image into a simple RGB array.
 *
 * @param yuyv YUYV format image to convert.
 * @param rgb Output image.
 * @return None.
 */
int ch_YUYV_to_RGB(const struct ch_frmbuf *yuyv, struct ch_frmbuf *rgb);

/**
 * @brief Convert an MJPG format image into a simple RGB array.
 *
 * @param mjpg MJPG format image to convert.
 * @param rgb Output image.
 * @return None.
 */
int ch_MJPG_to_RGB(const struct ch_frmbuf *mjpg, struct ch_frmbuf *rgb);

/**
 * @brief Initialize a decoding context for the device's video stream.
 *
 * @param device Device to initialize context for.
 * @return 0 on success, -1 on failure.
 */
int ch_init_decode_cx(struct ch_device *device);

/**
 * @brief Destroy allocated memory for a decoding context.
 *
 * @param device Device to destroy allocated context for.
 * @return None.
 */
void ch_destroy_decode_cx(struct ch_device *device);

/**
 * @brief Decode a device's video stream into a basic RGB24 pixel buffer.
 *
 * @param device Device to decode video for.
 * @return 0 on success, -1 on failure.
 */
int ch_decode(struct ch_device *device);

#ifdef __cplusplus
}
#endif

#endif
