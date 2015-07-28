#ifndef CHIASM_DECODE_H_
#define CHIASM_DECODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>

#include <chiasm/types.h>

// Backwards compatibility.
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc  avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

/**
 * @brief Calculate stride of an image from a format, width, alignment.
 *
 * @param width Width of the image in bytes.
 * @param alignment Alignment of the image in bytes.
 * @return The calculated image stride.
 */
uint32_t ch_calc_stride(struct ch_dl_cx *cx, uint32_t width, uint32_t alignment);

/**
 * @brief Initialize a plugin's output context.
 *
 * @param device Device the plugin is using.
 * @param cx The plugin's context.
 * @return 0 on success, -1 on failure.
 */
int ch_init_plugin_out(struct ch_device *device, struct ch_dl_cx *cx);

/**
 * @brief Destroy allocated context for a plugin.
 *
 * @param cx The context to deallocate.
 * @return None.
 */
void ch_destroy_plugin_out(struct ch_dl_cx *cx);

/**
 * @brief Initialize a decoding context for the device's video stream.
 *
 * @param device Device to initialize context for.
 * @param cx The decoding context to initialize.
 * @return 0 on success, -1 on failure.
 */
int ch_init_decode_cx(struct ch_device *device, struct ch_decode_cx *cx);

/**
 * @brief Destroy allocated memory for a decoding context.
 *
 * @param device Device to destroy allocated context for.
 * @param cx The decoding context to destroy.
 * @return None.
 */
void ch_destroy_decode_cx(struct ch_decode_cx *cx);

/**
 * @brief Decode a device's video stream into an uncompressed format.
 *
 * @param device Device to decode video for.
 * @param in_buf Input buffer of compressed video.
 * @param cx Decoding context to use.
 * @return 1 on a frame being decoded, 0 on success but no frame, -1 on failure.
 */
int ch_decode(struct ch_device *device, struct ch_frmbuf *in_buf,
              struct ch_decode_cx *cx);

/**
 * @brief Output an image in a requested format by a plugin.
 *
 * @param device Device to output video from.
 * @param decode Decoding context used.
 * @param cx Plugin output context to use.
 * @return 0 on success, -1 on failure.
 */
int ch_output(struct ch_device *device, struct ch_decode_cx *decode,
              struct ch_dl_cx *cx);

#ifdef __cplusplus
}
#endif

#endif
