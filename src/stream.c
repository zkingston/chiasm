#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>

#include <linux/types.h>
#include <linux/videodev2.h>

#include <chiasm.h>

#define CH_DEFAULT_DEVICE "/dev/video0"
#define CH_DEFAULT_FORMAT "YUYV"
#define CH_DEFAULT_WIDTH  320
#define CH_DEFAULT_HEIGHT 240
#define CH_DEFAULT_BUFNUM 5

bool list = false;

/**
 * Wrapper around ioctl() to print out error message upon failure.
 */
int
ioctl_r(int fd, int request, void *arg)
{
    if (ioctl(fd, request, arg) == -1) {
	if (errno != EINVAL)
	    fprintf(stderr, "ioctl failure. %d: %s\n", errno, strerror(errno));

	return (-1);
    }

    return (0);
}

/**
 * Converts a pixelformat code into a readable character buffer
 */
void
pixfmt_to_string(__u32 pixfmt, char *buf)
{
    size_t idx;
    for (idx = 0; idx < 4; idx++)
	buf[idx] = (pixfmt >> (8 * idx)) & 0xFF;

    buf[idx] = '\0';
}

/**
 * Convert a 4-character string into the pixelformat code.
 */
__u32
string_to_pixfmt(char *buf)
{
    __u32 pixfmt = 0;

    size_t idx;
    for (idx = 0; idx < 4; idx++)
	pixfmt |= (buf[idx] << (8 * idx));

    return (pixfmt);
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
query_fmts(int fd, __u32 pixelformat, __u32 frame_width, __u32 frame_height)
{
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl_r(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
	char pixfmt_buf[5];
	pixfmt_to_string(fmtdesc.pixelformat, pixfmt_buf);

	struct v4l2_frmsizeenum frmsize;

	frmsize.index = 0;
	frmsize.pixel_format = fmtdesc.pixelformat;

	bool check_fmt = false;
	if (list)
	    printf("Format %s:", pixfmt_buf);

	else if (pixelformat == fmtdesc.pixelformat)
	    check_fmt = true;

	while (ioctl_r(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
	    // Only supporting discrete resolutions currently.
	    if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		if (list) {
		    struct v4l2_frmivalenum frmival;

		    // Print out maximum framerate for each resolution.
		    frmival.index = 0;
		    frmival.pixel_format = fmtdesc.pixelformat;
		    frmival.width = frmsize.discrete.width;
		    frmival.height = frmsize.discrete.height;

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
		    if (frame_width == frmsize.discrete.width
			&& frame_height == frmsize.discrete.height)
			return (0);
	    }

	    frmsize.index++;
	}

	if (check_fmt) {
	    fprintf(stderr, "%ux%u is invalid geometry for format %s.\n",
		    frame_width, frame_height, pixfmt_buf);

	    return (-1);
	}

	if (list)
	    printf("\n");

	fmtdesc.index++;
    }

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
init_device(int fd, __u32 pixel_format, __u32 frame_width, __u32 frame_height)
{
    struct v4l2_format format;

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = frame_width;
    format.fmt.pix.height = frame_height;
    format.fmt.pix.pixelformat = pixel_format;
    format.fmt.pix.field = V4L2_FIELD_ANY;
    format.fmt.pix.bytesperline = 0;

    if (ioctl_r(fd, VIDIOC_S_FMT, &format) == -1) {
	fprintf(stderr, "Could not set output format.\n");
	return (-1);
    }

    return (0);
}

int
unmap_buffers(__u32 buffer_count, struct ch_buf *buffers)
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
init_mmap(int fd, __u32 buffer_count, struct ch_buf *buffers)
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
init_stream(int fd, __u32 buffer_count)
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
query_frame(int fd, __u32 buffer_count, struct ch_buf *buffers)
{
    struct v4l2_buffer buf;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl_r(fd, VIDIOC_DQBUF, &buf) == -1) {
	fprintf(stderr, "Failure dequeuing buffer.\n");
	return (-1);
    }

    if (buf.index > buffer_count) {
	fprintf(stderr, "Bad buffer index returned from deque.\n");
	return (-1);
    }

    fwrite(buffers[buf.index].start, buf.bytesused, 1, stdout);
    fflush(stdout);

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
    if ((fd = open(video_device, O_RDWR | O_NONBLOCK )) == -1) {
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
    int fd;
    char *video_device = CH_DEFAULT_DEVICE;
    __u32 pixel_format = string_to_pixfmt(CH_DEFAULT_FORMAT);
    __u32 frame_width  = CH_DEFAULT_WIDTH;
    __u32 frame_height = CH_DEFAULT_HEIGHT;
    __u32 buffer_count = CH_DEFAULT_BUFNUM;

    int opt;
    char *opts = "b:d:f:g:s:lh?";
    while ((opt = getopt(argc, argv, opts)) != -1) {
	switch (opt) {
	case 'd':
	    video_device = optarg;
	    break;
	case 'l':
	    list = true;
	    break;
	case 'b':
	    buffer_count = (__u32) strtoul(optarg, NULL, 10);
	    if (errno == EINVAL || errno == ERANGE || buffer_count == 0) {
		fprintf(stderr, "Invalid value in buffer count argument %s.\n",
			optarg);
		return (-1);
	    }

	    break;
	case 'f':
	    if (strnlen(optarg, 5) != 4) {
		fprintf(stderr, "Pixel formats must be 4 characters in length.\n");
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
		"  -d    Device name. \"%s\" by default.\n"
		"  -f    4-character image format code. %s by default.\n"
		"  -g    Frame geometry in <w>x<h> format. %ux%u by default.\n"
		"  -b    Specify number of buffers to request. %u by default.\n"
		"  -l    List formats, resolutions, framerates and exit.\n"
		"  -?,h  Show this help.\n",
		argv[0],
		CH_DEFAULT_DEVICE,
		CH_DEFAULT_FORMAT,
		CH_DEFAULT_WIDTH,
		CH_DEFAULT_HEIGHT,
		CH_DEFAULT_BUFNUM
	    );

	    return (0);
	}
    }

    struct ch_buf *buffers = calloc(buffer_count, sizeof(struct ch_buf));
    if (buffers == NULL || errno == ENOMEM) {
	fprintf(stderr, "No memory available.\n");
	return (-1);
    }

    int r = 0;

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

    for (;;) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	r = select(fd + 1, &fds, NULL, NULL, &timeout);

	if (r == -1) {
	    fprintf(stderr, "Error on select. %d: %s\n", errno, strerror(errno));
	    break;
	} else if (r == 0) {
	    fprintf(stderr, "Timeout on select.\n");
	    break;
	}

	if (query_frame(fd, buffer_count, buffers) == -1)
	    break;

	break;
    }

cleanup:
    stop_stream(fd);
    unmap_buffers(buffer_count, buffers);
    free(buffers);

    if (fd > 0)
	close_device(fd);


    return (r);
}
