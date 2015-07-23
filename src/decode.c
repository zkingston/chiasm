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

int
ch_set_out_pixfmt(struct ch_device *device, char *fmt)
{
    struct ch_decode_cx *cx = &device->decode_cx;

    if (strcmp(fmt, "GRAY8") == 0)
	cx->out_pixfmt = AV_PIX_FMT_GRAY8;
    else if (strcmp(fmt, "RGB24") == 0)
	cx->out_pixfmt = AV_PIX_FMT_RGB24;
    else {
	return (-1);
    }

    return (0);
}

int
ch_init_decode_cx(struct ch_device *device)
{
    struct ch_decode_cx *cx = &device->decode_cx;

    // Register codecs so they can be found.
    if (!ch_codec_registered) {
	av_register_all();
	ch_codec_registered = true;
    }

    // Get size needed for output buffer.
    device->out_buffer.length = avpicture_get_size(cx->out_pixfmt,
						   device->framesize.width,
						   device->framesize.height);

    // Allocate output buffer.
    device->out_buffer.start = ch_calloc(1, device->out_buffer.length);
    if (device->out_buffer.start == NULL)
	goto clean;

    // Setup I/O frames.
    cx->frame_in = av_frame_alloc();
    cx->frame_out = av_frame_alloc();

    if (cx->frame_in == NULL || cx->frame_out == NULL) {
	ch_error("Failed to allocate frames.");
	goto clean;
    }

    if (avpicture_fill((AVPicture *) cx->frame_out,
		       device->out_buffer.start, cx->out_pixfmt,
		       device->framesize.width, device->framesize.height) < 0) {
	ch_error("Failed to setup output frame fields.");
	goto clean;
    };

    enum AVCodecID codec_id = AV_CODEC_ID_NONE;
    cx->compressed = true;

    // Find codec ID based on input pixel format.
    switch (device->in_pixfmt) {
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
    ch_destroy_decode_cx(device);
    return (-1);
}

void
ch_destroy_decode_cx(struct ch_device *device)
{
    struct ch_decode_cx *cx = &device->decode_cx;

    device->out_buffer.length = 0;
    if (device->out_buffer.start != NULL)
	free(device->out_buffer.start);

    av_frame_free(&cx->frame_in);
    av_frame_free(&cx->frame_out);

    if (cx->codec_cx != NULL) {
        avcodec_close(cx->codec_cx);
        av_free(cx->codec_cx);
    }

    if (cx->sws_cx != NULL)
	sws_freeContext(cx->sws_cx);
}

int
ch_decode(struct ch_device *device)
{
    struct ch_decode_cx *cx = &device->decode_cx;

    enum AVPixelFormat in_pixfmt = AV_PIX_FMT_NONE;
    int finish = 0;

    if (!cx->compressed) {
	finish = 1;
	in_pixfmt = AV_PIX_FMT_YUYV422;

	if (avpicture_fill((AVPicture *) cx->frame_in,
			   device->in_buffer->start, AV_PIX_FMT_YUYV422,
			   device->framesize.width, device->framesize.height) < 0) {
	    ch_error("Failed to setup output frame fields.");
	    return (-1);

	}
    } else {
	// Initialize packet to use input buffer.
	AVPacket packet;
	av_init_packet(&packet);

	packet.data = device->in_buffer->start;
	packet.size = device->in_buffer->length;

	if (avcodec_decode_video2(cx->codec_cx, cx->frame_in,
				  &finish, &packet) < 0) {
	    ch_error("Failed decoding video.");
	    av_free_packet(&packet);
	    return (-1);
	}

	av_free_packet(&packet);
	in_pixfmt = cx->codec_cx->pix_fmt;
    }

    // Convert image into RGB24 upon success.
    if (finish) {
	// Allocate the SWS context if we have not already.
	if (cx->sws_cx == NULL) {
	    cx->sws_cx = sws_getContext(
		device->framesize.width,
		device->framesize.height,
		in_pixfmt,
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

	// Convert the image into a RGB24 array.
	sws_scale(
	    cx->sws_cx,
	    (uint8_t const * const *) cx->frame_in->data,
	    cx->frame_in->linesize,
	    0,
	    device->framesize.height,
	    cx->frame_out->data,
	    cx->frame_out->linesize
	);
    }

    return (0);
}
