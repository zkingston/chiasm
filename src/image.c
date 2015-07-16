#include <stdint.h>

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
