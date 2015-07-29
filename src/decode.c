#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <linux/videodev2.h>

#include <chiasm.h>

bool ch_codec_registered = false;

inline uint32_t
ch_calc_stride(struct ch_dl_cx *cx, uint32_t width, uint32_t alignment)
{
    cx->b_per_pix = avpicture_get_size(cx->out_pixfmt, 1, 1);
    uint32_t stride = width * cx->b_per_pix;

    if ((stride % alignment) != 0)
        stride += alignment - (stride % alignment);

    return (stride);
}

int
ch_init_plugin_out(struct ch_device *device, struct ch_dl_cx *cx)
{
    cx->b_per_pix = avpicture_get_size(cx->out_pixfmt, 1, 1);

    // If the output stride was uninitialized by the plugin, use the width.
    if (cx->out_stride == 0)
        cx->out_stride = device->framesize.width * cx->b_per_pix;

    uint32_t length = cx->out_stride * device->framesize.height;

    size_t idx;
    for (idx = 0; idx < CH_DL_NUMBUF; idx++) {
        // Get size needed for output buffer.
        cx->out_buffer[idx].length = length;

        // Allocate output buffer.
        cx->out_buffer[idx].start = ch_calloc(1, length);
        if (cx->out_buffer[idx].start == NULL)
            goto clean;
    }

    cx->frame_out = av_frame_alloc();
    if (cx->frame_out == NULL) {
        ch_error("Failed to allocated output frame.");
        goto clean;
    }

    return (0);

clean:
    ch_destroy_plugin_out(cx);
    return (-1);
}

void
ch_destroy_plugin_out(struct ch_dl_cx *cx)
{
    size_t idx;
    for (idx = 0; idx < CH_DL_NUMBUF; idx++)
        if (cx->out_buffer[idx].start)
            free(cx->out_buffer[idx].start);

    if (cx->frame_out)
        av_frame_free(&cx->frame_out);

    if (cx->sws_cx)
        sws_freeContext(cx->sws_cx);
}

int
ch_init_decode_cx(struct ch_device *device, struct ch_decode_cx *cx)
{
    // Register codecs so they can be found.
    if (!ch_codec_registered) {
	av_register_all();
	ch_codec_registered = true;
    }

    cx->codec_cx = NULL;

    // Setup I/O frames.
    cx->frame_in = av_frame_alloc();
    if (cx->frame_in == NULL)
        goto clean;

    enum AVCodecID codec_id = AV_CODEC_ID_NONE;

    // Find codec ID based on input pixel format.
    switch (device->in_pixfmt) {
    case V4L2_PIX_FMT_YUYV:
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

    // Find and allocate codec context.
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

    // For streaming codecs, specify that we might not be sending complete frames.
    if (codec->capabilities & CODEC_CAP_TRUNCATED)
        cx->codec_cx->flags |= CODEC_FLAG_TRUNCATED;

    cx->codec_cx->width = device->framesize.width;
    cx->codec_cx->height = device->framesize.height;

    if (avcodec_open2(cx->codec_cx, codec, NULL) < 0) {
	ch_error("Failed to open codec.");
	goto clean;
    }


    return (0);

clean:
    ch_destroy_decode_cx(cx);
    return (-1);
}

void
ch_destroy_decode_cx(struct ch_decode_cx *cx)
{
    if (cx->frame_in)
        av_frame_free(&cx->frame_in);

    if (cx->codec_cx != NULL) {
        avcodec_close(cx->codec_cx);
        av_free(cx->codec_cx);
    }
}

int
ch_decode(struct ch_device *device, struct ch_frmbuf *in_buf,
          struct ch_decode_cx *cx)
{
    int finish = 0;

    // Uncompressed stream.
    if (device->in_pixfmt == V4L2_PIX_FMT_YUYV) {
	if (avpicture_fill((AVPicture *) cx->frame_in,
			   in_buf->start, AV_PIX_FMT_YUYV422,
			   device->framesize.width, device->framesize.height) < 0) {
	    ch_error("Failed to setup output frame fields.");
	    return (-1);
	}

	finish = 1;
	cx->in_pixfmt = AV_PIX_FMT_YUYV422;

    } else {
	// Initialize packet to use input buffer.
	AVPacket packet;
	av_init_packet(&packet);

	packet.data = in_buf->start;
	packet.size = in_buf->length;

	if (avcodec_decode_video2(cx->codec_cx, cx->frame_in,
				  &finish, &packet) < 0) {
	    ch_error("Failed decoding video.");
	    av_free_packet(&packet);
	    return (-1);
	}

	av_free_packet(&packet);
	cx->in_pixfmt = cx->codec_cx->pix_fmt;
    }

    return (finish);
}

int
ch_output(struct ch_device *device, struct ch_decode_cx *decode,
          struct ch_dl_cx *cx)
{
    uint32_t idx = (cx->select + 1) % CH_DL_NUMBUF;

    if (0 > avpicture_fill((AVPicture *) cx->frame_out,
                           cx->out_buffer[idx].start,
                           cx->out_pixfmt, cx->out_stride / cx->b_per_pix,
                           device->framesize.height)) {
        ch_error("Failed to setup output frame fields.");
        return (-1);
    }

    if (cx->sws_cx == NULL) {
        cx->sws_cx = sws_getContext(
            device->framesize.width,
            device->framesize.height,
            decode->in_pixfmt,
            device->framesize.width,
            device->framesize.height,
            cx->out_pixfmt,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
	);

        if (cx->sws_cx == NULL) {
            ch_error("Failed to initialize SWS context.");
            return (-1);
        }
    }

    sws_scale(
        cx->sws_cx,
        (uint8_t const * const *) decode->frame_in->data,
        decode->frame_in->linesize,
        0,
        device->framesize.height,
        cx->frame_out->data,
        cx->frame_out->linesize
    );

    cx->nonce[idx] = cx->nonce[cx->select] + 1;

    return (0);
}
