#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <errno.h>
#include <string.h>

#include <jpeglib.h>

#include <linux/videodev2.h>

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
	ch_error_no("Failed to open memory buffer.", errno);
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
	ch_error_no("Failed to close memory buffer.", errno);
	r = -1;
    }

    // Emit warning and debug messages upon error.
    if (jerr.pub.num_warnings != 0) {
	ch_error("Encountered errors in JPEG decompression.");

        int idx;
	for (idx = 0; idx < jerr.pub.last_jpeg_message; idx++)
	    ch_error(jerr.pub.jpeg_message_table[idx]);

	r = -1;
    }

    return (r);
}

#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

bool ch_codec_registered = false;

int
ch_init_decode_cx(struct ch_device *device)
{
    struct ch_decode_cx *cx = &device->decode_cx;

    if (!ch_codec_registered) {
	av_register_all();
	ch_codec_registered = true;
    }

    enum AVCodecID codec_id = AV_CODEC_ID_NONE;

    cx->compressed = true;

    switch (device->pixelformat) {
    case V4L2_PIX_FMT_YUYV:
	cx->compressed = false;
	return (0);

    case V4L2_PIX_FMT_MJPEG:
	codec_id = AV_CODEC_ID_MJPEG;
	break;

    case V4L2_PIX_FMT_H264:
	codec_id = AV_CODEC_ID_H264;
	break;

    default:
	ch_error("Unsupported format.");
	return (-1);
    }

    AVCodec *codec = avcodec_find_decoder(codec_id);
    if (codec == NULL) {
	ch_error("Failed to find requested codec.");
        return (-1);
    }

    cx->codec_cx = avcodec_alloc_context3(codec);
    if (cx->codec_cx == NULL) {
	ch_error("Failed to allocate codec context.");
	return (-1);
    }

    cx->codec_cx->width = device->framesize.width;
    cx->codec_cx->height = device->framesize.height;

    if (avcodec_open2(cx->codec_cx, codec, NULL) < 0) {
	ch_error("Failed to open codec.");
	goto clean;
    }

    cx->frame_in = av_frame_alloc();
    cx->frame_out = av_frame_alloc();

    if (cx->frame_in == NULL || cx->frame_out == NULL) {
	ch_error("Failed to allocate frames.");
	goto clean;
    }

    if (avpicture_fill((AVPicture *) cx->frame_out,
		       device->out_buffer.start, AV_PIX_FMT_RGB24,
		       cx->codec_cx->width, cx->codec_cx->height) < 0) {
	ch_error("Failed to setup output frame fields.");
	goto clean;
    };

    return (0);

clean:
    ch_destroy_decode_cx(device);
    return (-1);
}

void
ch_destroy_decode_cx(struct ch_device *device)
{
    struct ch_decode_cx *cx = &device->decode_cx;

    av_frame_free(&cx->frame_in);
    av_frame_free(&cx->frame_out);

    if (cx->codec_cx != NULL) {
        avcodec_close(cx->codec_cx);
        av_free(cx->codec_cx);
    }
}

int
ch_decode(struct ch_device *device)
{
    struct ch_decode_cx *cx = &device->decode_cx;

    if (!cx->compressed) {
	ch_YUYV_to_RGB(device->in_buffer, &device->out_buffer);
	return (0);
    }

    int finish = 0;

    AVPacket packet;
    av_init_packet(&packet);

    packet.data = device->in_buffer->start;
    packet.size = device->in_buffer->length;

    int r = 0;
    if (avcodec_decode_video2(cx->codec_cx, cx->frame_in, &finish, &packet) < 0) {
	ch_error("Failed decoding video.");
	r = -1;
	goto exit;
    }

    if (finish) {
	struct SwsContext *sws_cx = sws_getContext(
	    cx->codec_cx->width,
	    cx->codec_cx->height,
	    cx->codec_cx->pix_fmt,
	    cx->codec_cx->width,
	    cx->codec_cx->height,
	    AV_PIX_FMT_RGB24,
	    SWS_BILINEAR,
	    NULL,
	    NULL,
	    NULL
	);

	if (sws_cx == NULL) {
	    ch_error("Failed to initialize SWS context.");
	    r = -1;
	    goto exit;
	}

	sws_scale(
	    sws_cx,
	    (uint8_t const * const *) cx->frame_in->data,
	    cx->frame_in->linesize,
	    0,
	    cx->codec_cx->height,
	    cx->frame_out->data,
	    cx->frame_out->linesize
	);

	sws_freeContext(sws_cx);

    } else {
	ch_error("No frame received.");
	r = -1;
    }

exit:
    av_free_packet(&packet);

    return (r);
}
