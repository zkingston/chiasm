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
