#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>

#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>

#include <linux/types.h>
#include <linux/videodev2.h>

#include <chiasm.h>

bool list = false;

/**
 * Wrapper around ioctl() to print out error message upon failure.
 */
static int
ioctl_r(int fd, int request, void *arg)
{
    int r;

    do {
	r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);

    if (r == -1) {
	if (errno != EINVAL)
	    fprintf(stderr, "ioctl failure. %d: %s\n", errno, strerror(errno));

	return (-1);
    }

    return (0);
}

/**
 * Clamps a double to byte value.
 */
static inline uint8_t
byte_clamp(double v)
{
    return (uint8_t) ((v > 255) ? 255 : ((v < 0) ? 0 : v));
}

/**
 * Converts a number of seconds into a struct timeval.
 */
struct timeval
seconds_to_timeval(double seconds)
{
    struct timeval ret;
    ret.tv_sec = (long) seconds;
    ret.tv_usec = (long) ((seconds - (double) ret.tv_sec) * 1000000);

    return (ret);
}

/**
 * Converts a YUYV image into RGB.
 */
static int
YUYV_to_RGB(struct ch_frmbuf *yuyv, struct ch_frmbuf *rgb)
{
    // rgb->length = yuyv->length / 2 * 3;
    // rgb->start = calloc_r(rgb->length, sizeof(uint8_t));

    if (rgb->start == NULL)
	return (-1);

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

	rgb->start[idx / 2 * 3 + 0] = byte_clamp(R);
	rgb->start[idx / 2 * 3 + 1] = byte_clamp(G);
	rgb->start[idx / 2 * 3 + 2] = byte_clamp(B);
    }

    return (0);
}

/**
 * Read a frame from one of the buffers.
 */
int
query_frame(int fd, uint32_t buffer_count, struct ch_frmbuf *buffers)
{
    struct v4l2_buffer buf;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Deque buffer from device.
    if (ioctl_r(fd, VIDIOC_DQBUF, &buf) == -1) {
	fprintf(stderr, "Failure dequeuing buffer.\n");
	return (-1);
    }

    // Verify buffer is valid.
    if (buf.index > buffer_count) {
	fprintf(stderr, "Bad buffer index returned from deque.\n");
	return (-1);
    }

    // Output image.
    // struct ch_frmbuf t;
    // YUYV_to_RGB(&buffers[buf.index], &t);

    // fwrite(buffers[buf.index].start, buf.bytesused, 1, stdout);
    // fwrite(t.start, t.length, 1, stdout);
    fprintf(stdout, ".");
    fflush(stdout);

    // Requeue buffer.
    if (ioctl_r(fd, VIDIOC_QBUF, &buf) == -1) {
	fprintf(stderr, "Failed requeuing buffer.\n");
	return (-1);
    }

    return (0);
}

static int
list_formats(struct ch_device *device)
{
    struct ch_fmts *fmts = ch_enum_fmts(device);
    if (fmts == NULL)
	return (-1);

    size_t idx;
    for (idx = 0; idx < fmts->length; idx++) {
	char pixfmt_buf[5];
	ch_pixfmt_to_string(fmts->fmts[idx], pixfmt_buf);

	printf("%s:", pixfmt_buf);

	device->pixelformat = fmts->fmts[idx];
	struct ch_frmsizes *frmsizes = ch_enum_frmsizes(device);
	if (frmsizes == NULL)
	    return (-1);

	size_t jdx;
	for (jdx = 0; jdx < frmsizes->length; jdx++)
	    printf(" %ux%u",
		   frmsizes->frmsizes[jdx].width,
		   frmsizes->frmsizes[jdx].height);

	ch_destroy_frmsizes(frmsizes);
	printf("\n");
    }

    ch_destroy_fmts(fmts);
    fflush(stdout);

    return (0);
}

int
main(int argc, char *argv[])
{
    char *video_device = CH_DEFAULT_DEVICE;
    uint32_t pixel_format = ch_string_to_pixfmt(CH_DEFAULT_FORMAT);
    uint32_t frame_width  = CH_DEFAULT_WIDTH;
    uint32_t frame_height = CH_DEFAULT_HEIGHT;
    uint32_t buffer_count = CH_DEFAULT_BUFNUM;
    size_t num_frames  = CH_DEFAULT_NUMFRAMES;
    struct timeval timeout = seconds_to_timeval(CH_DEFAULT_TIMEOUT);

    int opt;
    char *opts = "n:t:b:d:f:g:s:lh?";
    while ((opt = getopt(argc, argv, opts)) != -1) {
	switch (opt) {
	case 'd':
	    video_device = optarg;
	    break;
	case 'l':
	    list = true;
	    break;
	case 'n':
	    num_frames = strtoul(optarg, NULL, 10);
	    if (errno == EINVAL || errno == ERANGE) {
		fprintf(stderr, "Invalid number of frames %s.\n", optarg);
		return (-1);
	    }

	    break;
	case 't': {
	    char *ptr;
	    double r = strtod(optarg, &ptr);

	    if (r == 0 && ptr == optarg) {
		fprintf(stderr, "Invalid timeout.\n");
		return (-1);
	    }

	    timeout = seconds_to_timeval(r);
	    break;
	}
	case 'b':
	    buffer_count = (uint32_t) strtoul(optarg, NULL, 10);
	    if (errno == EINVAL || errno == ERANGE || buffer_count == 0) {
		fprintf(stderr, "Invalid value in buffer count argument %s.\n",
			optarg);
		return (-1);
	    }

	    break;
	case 'f':
	    if (strnlen(optarg, 5) > 4) {
		fprintf(stderr, "Pixel formats must be at most 4 characters.\n");
		return (-1);
	    }

	    pixel_format = ch_string_to_pixfmt(optarg);
	    break;
	case 'g':
	    if (sscanf(optarg, "%ux%u", &frame_width, &frame_height) != 2) {
		fprintf(stderr, "Error parsing geometry string.\n");
		return (-1);
	    }

	    break;
	case 'h':
	case '?':
	default:
	    printf(
		"Usage: %s [OPTIONS]\n"
		"Stream video device to ach channel.\n"
		"Options:\n"
		" -d    Device name. \"%s\" by default.\n"
		" -f    Image format code. %s by default.\n"
		" -g    Frame geometry in <w>x<h> format. %ux%u by default.\n"
		" -b    Specify number of buffers to request. %u by default.\n"
		" -t    Timeout in seconds. %f by default.\n"
		" -n    Number of frames to read. Infinite if 0. %d by default.\n"
		" -l    List formats, resolutions, framerates and exit.\n"
		" -?,h  Show this help.\n",
		argv[0],
		CH_DEFAULT_DEVICE,
		CH_DEFAULT_FORMAT,
		CH_DEFAULT_WIDTH,
		CH_DEFAULT_HEIGHT,
		CH_DEFAULT_BUFNUM,
		CH_DEFAULT_TIMEOUT,
		CH_DEFAULT_NUMFRAMES
	    );

	    return (0);
	}
    }

    int r = 0;
    int fd;

    struct ch_device device;
    ch_init_device(&device);

    device.name = video_device;

    if ((r = ch_open_device(&device)) == -1)
     	goto cleanup;

    // List all formats and framesizes then exit.
    if (list) {
	list_formats(&device);
	goto cleanup;
    }

    device.pixelformat = pixel_format;
    device.framesize.width = frame_width;
    device.framesize.height = frame_height;

    if (ch_set_fmt(&device) == -1)
	goto cleanup;

    device.num_buffers = buffer_count;

    if (ch_init_stream(&device) == -1)
	goto cleanup;

    if (ch_start_stream(&device) == -1)
	goto cleanup;

    fd = device.fd;

    // Only grab as many frames as desired.
    size_t n;
    for (n = 0; (num_frames != 0) ? n < num_frames : 1; n++) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	struct timeval temp = timeout;
	r = select(fd + 1, &fds, NULL, NULL, &temp);

	if (r == -1) {
	    fprintf(stderr, "Error on select. %d: %s\n", errno, strerror(errno));
	    break;
	} else if (r == 0) {
	    fprintf(stderr, "Timeout on select.\n");
	    break;
	}

	if ((r = query_frame(fd, buffer_count, device.in_buffers)) == -1)
	    break;
    }

cleanup:
    ch_close_device(&device);

    return (r);
}
