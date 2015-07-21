#ifndef CHIASM_DECODE_H_
#define CHIASM_DECODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <setjmp.h>

#include <jpeglib.h>
#include <libavcodec/avcodec.h>

// Backwards compatibility.
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc  avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

struct ch_jpeg_error_cx {
    struct jpeg_error_mgr pub;
    jmp_buf cx;
};

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

#ifdef __cplusplus
}
#endif

#endif
