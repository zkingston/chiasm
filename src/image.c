#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

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

void
ch_YUYV_to_RGB(struct ch_frmbuf *yuyv, struct ch_frmbuf *rgb)
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
}

// TODO: Fix.
void
ch_MJPG_to_RGB(struct ch_frmbuf *mjpg, struct ch_frmbuf *rgb)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);

    FILE *jsrc = fmemopen(mjpg->start, mjpg->length, "rb");

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, jsrc);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    JSAMPARRAY buffer;
    int row_stride;

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
	((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    size_t idx = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
	jpeg_read_scanlines(&cinfo, buffer, 1);

	memcpy(&rgb->start[idx], buffer[0], row_stride);
	idx += row_stride;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(jsrc);
}