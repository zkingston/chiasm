#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <errno.h>
#include <string.h>

#include <jpeglib.h>

#include <chiasm.h>

/**
 * @brief Clamp a double value to a unsigned byte value.
 *
 * @param v Value to clamp.
 * @return Clamped value.
 */
static inline uint8_t
ch_byte_clamp(double v)
{
    return (uint8_t) ((v > 255) ? 255 : ((v < 0) ? 0 : v));
}

int
ch_YUYV_to_RGB(const struct ch_frmbuf *yuyv, struct ch_frmbuf *rgb)
{
    size_t idx;
    for (idx = 0; idx < yuyv->length; idx += 2) {
	int u_off = (idx % 4 == 0) ? 1 : -1;
	int v_off = (idx % 4 == 2) ? 1 : -1;

	uint8_t y = yuyv->start[idx];

	uint8_t u = (idx + u_off > 0 && idx + u_off < yuyv->length)
	    ? yuyv->start[idx + u_off] : 0x80;

	uint8_t v = (idx + v_off > 0 && idx + v_off < yuyv->length)
	    ? yuyv->start[idx + v_off] : 0x80;

	double Y  = (255.0 / 219.0) * (y - 0x10);
	double Cb = (255.0 / 224.0) * (u - 0x80);
	double Cr = (255.0 / 224.0) * (v - 0x80);

	double R = 1.000 * Y + 0.000 * Cb + 1.402 * Cr;
	double G = 1.000 * Y + 0.344 * Cb - 0.714 * Cr;
	double B = 1.000 * Y + 1.772 * Cb + 0.000 * Cr;

	rgb->start[idx / 2 * 3 + 0] = ch_byte_clamp(R);
	rgb->start[idx / 2 * 3 + 1] = ch_byte_clamp(G);
	rgb->start[idx / 2 * 3 + 2] = ch_byte_clamp(B);
    }

    return (0);
}

/**
 * @brief Error handler for libjpeg decompression. Jumps back into context.
 *
 * @param cinfo Context information.
 * @return None.
 */

/**
static void
ch_jpeg_error(j_common_ptr cinfo)
{
    struct ch_jpeg_error_cx *jerr = (struct ch_jpeg_error_cx *) cinfo;
    (*jerr->pub.output_message)(cinfo);

    longjmp(jerr->cx, 1);
}


int
ch_MJPG_to_RGB(const struct ch_frmbuf *mjpg, struct ch_frmbuf *rgb)
{
    struct jpeg_decompress_struct cinfo;
    struct ch_jpeg_error_cx jerr;

    // Open jpeg image in memory as a file source.
    FILE *jsrc = fmemopen(mjpg->start, mjpg->length, "rb");
    if (jsrc == NULL) {
	fprintf(stderr, "Failed to open memory buffer. %d: %s\n",
		errno, strerror(errno));
	return (-1);
    }

    // Initialize error handling. Using setjmp to return control in error.
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ch_jpeg_error;

    int r = 0;
    if ((r = setjmp(jerr.cx)) == -1)
	goto exit;

    // Initialize decompression.
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, jsrc);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int row_stride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)
	((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    // Read all scanlines into RGB buffer.
    {
	size_t idx = 0;
	while (cinfo.output_scanline < cinfo.output_height) {
	    jpeg_read_scanlines(&cinfo, buffer, 1);

	    memcpy(&rgb->start[idx], buffer[0], row_stride);
	    idx += row_stride;
	}
    }

    jpeg_finish_decompress(&cinfo);

exit:
    jpeg_destroy_decompress(&cinfo);
    if (fclose(jsrc) == EOF) {
	fprintf(stderr, "Failed to close memory buffer. %d: %s.\n",
		errno, strerror(errno));
    };

    // Emit warning and debug messages upon error.
    if (jerr.pub.num_warnings != 0) {
	fprintf(stderr, "Encountered errors in JPEG decompression.\n");
	int idx;
	for (idx = 0; idx < jerr.pub.last_jpeg_message; idx++)
	    fprintf(stderr, "%s\n", jerr.pub.jpeg_message_table[idx]);
	r = -1;
    }

    return (r);
}
*/

#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

int
ch_MJPG_to_RGB(const struct ch_frmbuf *mjpg, struct ch_frmbuf *rgb)
{
    fprintf(stderr, "Registering codecs.\n");

    // Register all codecs.
    av_register_all();

    fprintf(stderr, "Finding codec.\n");

    // Find decoder for MJPEG.
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
    if (codec == NULL) {
	fprintf(stderr, "Codec not found.\n");
	return (-1);
    }

    fprintf(stderr, "Opening memory source.\n");

    FILE *src = fmemopen(mjpg->start, mjpg->length, "rb");
    if (src == NULL) {
	fprintf(stderr, "Failed to open memory buffer. %d: %s\n",
		errno, strerror(errno));
	return (-1);
    }

    fprintf(stderr, "Allocating codec context.\n");

    // Allocate codec context.
    int r = 0;
    AVCodecContext *codec_cx = avcodec_alloc_context3(codec);
    if (codec_cx == NULL) {
	fprintf(stderr, "Could not allocate codec context.\n");
	r = -1;
	goto exit;
    }

    fprintf(stderr, "Opening codec.\n");

    // Open codec.
    if (avcodec_open2(codec_cx, codec, NULL) < 0) {
	fprintf(stderr, "Could not open codec.\n");
	r = -1;
	goto exit;
    }

    AVFrame frame = av_frame_alloc();
    if (frame == NULL) {
	fprintf(stderr, "Failed to allocate frame.\n");
	r = -1;
	goto exit;
    }

    AVFrame frameRGB = av_frame_alloc();
    if (frameRGB == NULL) {
	fprintf(stderr, "Failed to allocate frame.\n");
	r = -1;
	goto exit;
    }

exit:
    fprintf(stderr, "Exiting decode.\n");

    av_frame_free(&frame);
    av_frame_free(&frameRGB);

    if (codec_cx != NULL) {
	avcodec_close(codec_cx);
	av_free(codec_cx);
    }

    if (fclose(src) == EOF)  {
	fprintf(stderr, "Failed to close memory source. %d: %s\n",
		errno, strerror(errno));
	r = -1;
    }

    return (r);
}
