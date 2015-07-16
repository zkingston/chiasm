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
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>

#include <linux/types.h>
#include <linux/videodev2.h>

#include <chiasm.h>

#define CH_DEFAULT_DEVICE    "/dev/video0"
#define CH_DEFAULT_FORMAT    "YUYV"
#define CH_DEFAULT_WIDTH     320
#define CH_DEFAULT_HEIGHT    240
#define CH_DEFAULT_BUFNUM    5
#define CH_DEFAULT_TIMEOUT   2.0
#define CH_DEFAULT_NUMFRAMES 0

struct ch_buf {
    uint8_t  *start;
    uint32_t length;
};

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

static void *
calloc_r(size_t nmemb, size_t size)
{
    void *r = calloc(nmemb, size);

    if (r == NULL || errno == ENOMEM) {
	fprintf(stderr, "No memory available.\n");
	return (NULL);
    }

    return (r);
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
 * Converts a pixelformat code into a readable character buffer
 */
static void
pixfmt_to_string(uint32_t pixfmt, char *buf)
{
    size_t idx;
    for (idx = 0; idx < 4; idx++)
	buf[idx] = (pixfmt >> (8 * idx)) & 0xFF;

    buf[idx] = '\0';
}

/**
 * Convert a pixel format string into the pixelformat code.
 */
static uint32_t
string_to_pixfmt(char *buf)
{
    uint32_t pixfmt = 0;

    size_t idx;
    for (idx = 0; idx < 4 && buf[idx] != '\0'; idx++)
	pixfmt |= (buf[idx] << (8 * idx));

    return (pixfmt);
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
YUYV_to_RGB(struct ch_buf *yuyv, struct ch_buf *rgb)
{
    rgb->length = yuyv->length / 2 * 3;
    rgb->start = calloc_r(rgb->length, sizeof(uint8_t));

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
 * Query the capabilities of a device, verify support.
 */
int
query_caps(int fd)
{
    struct v4l2_capability caps;

    if (ioctl_r(fd, VIDIOC_QUERYCAP, &caps) == -1)
	return (-1);

    // Verify video capture is supported.
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
	fprintf(stderr, "Device does not support video capture.\n");
	return (-1);
    }

    // Verify streaming is supported.
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
	fprintf(stderr, "Device does not support streaming I/O.\n");
	return (-1);
    }

    return (0);
}

/**
 * Query available formats for a device. Validate user specified
 * format and resolution.
 */
int
query_fmts(int fd, uint32_t pixelformat, uint32_t frame_width, uint32_t frame_height)
{
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    // Iterate until device returns no more.
    while (ioctl_r(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
	char pixfmt_buf[5];
	pixfmt_to_string(fmtdesc.pixelformat, pixfmt_buf);

	// Either print out information or check for available geometry.
	bool check_fmt = false;
	if (list)
	    printf("Format %s:", pixfmt_buf);

	else if (pixelformat == fmtdesc.pixelformat)
	    check_fmt = true;

	struct v4l2_frmsizeenum frmsize;

	frmsize.index = 0;
	frmsize.pixel_format = fmtdesc.pixelformat;

	// Iterate over framesizes for the current format until no more.
	while (ioctl_r(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
	    // Only supporting discrete resolutions currently.
	    if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		if (list) {
		    struct v4l2_frmivalenum frmival;

		    frmival.index = 0;
		    frmival.pixel_format = fmtdesc.pixelformat;
		    frmival.width = frmsize.discrete.width;
		    frmival.height = frmsize.discrete.height;

		    // Grab only the first frameinterval. Should be the highest.
		    if (ioctl_r(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) != -1)
			if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
			    printf(
				" %ux%u(%u/%u fps)",
				frmsize.discrete.width,
				frmsize.discrete.height,
				frmival.discrete.numerator,
				frmival.discrete.denominator
			    );

		} else if (check_fmt)
		    // If geometry for format is valid, return out.
		    if (frame_width == frmsize.discrete.width
			&& frame_height == frmsize.discrete.height)
			return (0);
	    }

	    frmsize.index++;
	}

	// If geometry was found, return error.
	if (check_fmt) {
	    fprintf(stderr, "%ux%u is invalid geometry for format %s.\n",
		    frame_width, frame_height, pixfmt_buf);

	    return (-1);
	}

	if (list)
	    printf("\n");

	fmtdesc.index++;
    }

    // If format was found, return error.
    if (!list) {
	char in_pixfmt_buf[5];
	pixfmt_to_string(pixelformat, in_pixfmt_buf);
	fprintf(stderr, "Invalid format %s.\n", in_pixfmt_buf);

	return (-1);
    }

    return (0);
}

/**
 * Initialize device state.
 */
int
init_device(int fd, uint32_t pixel_format, uint32_t frame_width, uint32_t frame_height)
{
    struct v4l2_format format;

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = frame_width;
    format.fmt.pix.height = frame_height;
    format.fmt.pix.pixelformat = pixel_format;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    format.fmt.pix.bytesperline = 0;

    if (ioctl_r(fd, VIDIOC_S_FMT, &format) == -1) {
	fprintf(stderr, "Could not set output format.\n");
	return (-1);
    }

    return (0);
}

/**
 * Unmap mmap-ed buffers.
 */
int
unmap_buffers(uint32_t buffer_count, struct ch_buf *buffers)
{
    size_t idx;
    for (idx = 0; idx < buffer_count; idx++) {
	if (buffers[idx].start == NULL)
	    continue;

	if (munmap(buffers[idx].start, buffers[idx].length) == -1) {
	    fprintf(stderr, "Failed to munmap buffer. %d: %s\n",
		    errno, strerror(errno));

	    return (-1);
	}
    }

    return (0);
}

/**
 * Initialize memory-mapped region for streaming video.
 */
int
init_mmap(int fd, uint32_t buffer_count, struct ch_buf *buffers)
{
    struct v4l2_requestbuffers req;

    req.count = buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    // Request a number of buffers.
    if (ioctl_r(fd, VIDIOC_REQBUFS, &req) == -1) {
	fprintf(stderr, "Failed to request buffers.\n");
	return (-1);
    }

    // Compare return to requested amount of buffers.
    if (req.count != buffer_count) {
	fprintf(stderr, "Insufficient buffer memory on device (%u vs. %u).\n",
		buffer_count, req.count);
	return (-1);
    }

    // Query each buffer and map it into our address space.
    size_t idx;
    for (idx = 0; idx < buffer_count; idx++) {
	struct v4l2_buffer buf;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = idx;

	if (ioctl_r(fd, VIDIOC_QUERYBUF, &buf) == -1) {
	    fprintf(stderr, "Failed to query buffers.\n");
	    return (-1);
	}

	buffers[idx].length = buf.length;
	buffers[idx].start = mmap(
	    NULL,
	    buf.length,
	    PROT_READ | PROT_WRITE,
	    MAP_SHARED,
	    fd,
	    buf.m.offset
	);

	if (buffers[idx].start == MAP_FAILED) {
	    fprintf(stderr, "Failed to mmap buffer. %d: %s\n",
		    errno, strerror(errno));

	    unmap_buffers(idx, buffers);

	    return (-1);
	}
    }

    return (0);
}

/**
 * Initialize streaming of the device.
 */
int
init_stream(int fd, uint32_t buffer_count)
{
    size_t idx;
    for (idx = 0; idx < buffer_count; idx++) {
	struct v4l2_buffer buf;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = idx;

	// Query each buffer to be filled.
	if (ioctl_r(fd, VIDIOC_QBUF, &buf) == -1) {
	    fprintf(stderr, "Failed to request buffer.\n");
	    return (-1);
	}
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl_r(fd, VIDIOC_STREAMON, &type) == -1) {
	fprintf(stderr, "Failed to start stream.\n");
	return (-1);
    }

    return (0);
}

/**
 * Stop streaming from the device.
 */
int
stop_stream(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl_r(fd, VIDIOC_STREAMOFF, &type) == -1) {
	fprintf(stderr, "Failed to stop stream.\n");
	return (-1);
    }

    return (0);
}

/**
 * Read a frame from one of the buffers.
 */
int
query_frame(int fd, uint32_t buffer_count, struct ch_buf *buffers)
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
    struct ch_buf t;
    YUYV_to_RGB(&buffers[buf.index], &t);

    // fwrite(buffers[buf.index].start, buf.bytesused, 1, stdout);
    fwrite(t.start, t.length, 1, stdout);
    fflush(stdout);

    // Requeue buffer.
    if (ioctl_r(fd, VIDIOC_QBUF, &buf) == -1) {
	fprintf(stderr, "Failed requeuing buffer.\n");
	return (-1);
    }

    return (0);
}

/**
 * Open a video device. Returns file descriptor on success, -1 on failure.
 */
int
open_device(char *video_device)
{
    struct stat st;

    // Verify existence of device.
    if (stat(video_device, &st) == -1) {
	fprintf(stderr, "Failed to find device. %d: %s\n",
		errno, strerror(errno));
	return (-1);
    }

    // Verify device is a character device.
    if (!S_ISCHR(st.st_mode)) {
	fprintf(stderr, "%s is not a character device.\n", video_device);
	return (-1);
    }

    int fd;

    // Open device in read/write non-blocking mode.
    if ((fd = open(video_device, O_RDWR | O_NONBLOCK)) == -1) {
	fprintf(stderr, "Failed to open device. %d: %s\n",
		errno, strerror(errno));
	return (-1);
    }

    return (fd);
}

/**
 * Close a video device. Returns 0 on success, -1 on failure.
 */
int
close_device(int fd)
{
    if (close(fd) == -1) {
	fprintf(stderr, "Failed to close device. %d: %s\n",
		errno, strerror(errno));
	return (-1);
    }

    return (0);
}

int
main(int argc, char *argv[])
{
    char *video_device = CH_DEFAULT_DEVICE;
    uint32_t pixel_format = string_to_pixfmt(CH_DEFAULT_FORMAT);
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

	    pixel_format = string_to_pixfmt(optarg);
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
    bool stream = false;
    struct ch_buf *buffers = calloc_r(buffer_count, sizeof(struct ch_buf));
    if (buffers == NULL)
	return (-1);

    if ((r = (fd = open_device(video_device))) == -1)
	goto cleanup;

    if ((r = query_caps(fd)) == -1)
	goto cleanup;

    if ((r = query_fmts(fd, pixel_format, frame_width, frame_height)) == -1)
	goto cleanup;

    if (list)
	goto cleanup;

    if ((r = init_device(fd, pixel_format, frame_width, frame_height)) == -1)
	goto cleanup;

    if ((r = init_mmap(fd, buffer_count, buffers)) == -1)
	goto cleanup;

    if ((r = init_stream(fd, buffer_count)) == -1)
	goto cleanup;

    stream = true;

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

	if ((r = query_frame(fd, buffer_count, buffers)) == -1)
	    break;
    }

cleanup:
    if (stream)
	stop_stream(fd);

    unmap_buffers(buffer_count, buffers);
    free(buffers);

    if (fd > 0)
	close_device(fd);

    return (r);
}
