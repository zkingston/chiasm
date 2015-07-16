#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include <chiasm.h>

/**
 * @brief Robust wrapper around ioctl.
 *
 * @param fd File-descriptor.
 * @param request ioctl request.
 * @param arg Arguments to request.
 * @return 0 on success, -1 on failure.
 */
static inline int
ch_ioctl(int fd, int request, void *arg)
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
 * @brief Robust wrapper around calloc.
 *
 * @param nmemb Number of elements to allocate.
 * @param size Size of each element.
 * @return Pointer to allocated memory on success, NULL on failure.
 */
static inline void *
ch_calloc(size_t nmemb, size_t size)
{
    void *r = calloc(nmemb, size);

    if (r == NULL || errno == ENOMEM) {
	fprintf(stderr, "No memory available.\n");
	return (NULL);
    }

    return (r);
}

inline void
ch_pixfmt_to_string(uint32_t pixfmt, char *buf)
{
    size_t idx;
    for (idx = 0; idx < 4; idx++)
	buf[idx] = (pixfmt >> (8 * idx)) & 0xFF;

    buf[idx] = '\0';
}

inline uint32_t
ch_string_to_pixfmt(char *buf)
{
    uint32_t pixfmt = 0;

    size_t idx;
    for (idx = 0; idx < 4 && buf[idx] != '\0'; idx++)
	pixfmt |= (buf[idx] << (8 * idx));

    return (pixfmt);
}

inline struct timeval
ch_sec_to_timeval(double seconds)
{
    struct timeval ret;
    ret.tv_sec = (long) seconds;
    ret.tv_usec = (long) ((seconds - (double) ret.tv_sec) * 1000000);

    return (ret);
}

/**
 * @brief Checks a video device's capabilities for needed support.
 *
 * @param fd File-desciptor of device to check.
 * @return 0 on success, -1 on failure.
 */
static int
ch_validate_device(int fd)
{
    struct v4l2_capability caps;

    // Query device for capabilities.
    if (ch_ioctl(fd, VIDIOC_QUERYCAP, &caps) == -1)
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
 * @brief Unmaps memory-mapped streaming buffers.
 *
 * @param device Device to unmap from.
 * @return 0 on success, -1 on failure.
 */
static int
ch_unmap_buffers(struct ch_device *device)
{
    size_t idx;
    for (idx = 0; idx < device->num_buffers; idx++) {
	if (device->in_buffers[idx].start == NULL)
	    continue;

	if (munmap(device->in_buffers[idx].start,
		   device->in_buffers[idx].length) == -1) {
	    fprintf(stderr, "Failed to munmap buffer. %d: %s\n",
		    errno, strerror(errno));

	    return (-1);
	}
    }

    return (0);
}

void
ch_init_device(struct ch_device *device)
{
    device->name = CH_DEFAULT_DEVICE;
    device->fd = 0;

    device->in_buffers = NULL;
    device->num_buffers = CH_DEFAULT_BUFNUM;

    device->out_buffer.start = NULL;
    device->out_buffer.length = 0;

    pthread_mutex_init(&device->out_mutex, NULL);

    device->framesize = (struct ch_rect) {CH_DEFAULT_WIDTH, CH_DEFAULT_HEIGHT};
    device->pixelformat = ch_string_to_pixfmt(CH_DEFAULT_FORMAT);
    device->timeout = ch_sec_to_timeval(CH_DEFAULT_TIMEOUT);
    device->stream = false;
}

int
ch_open_device(struct ch_device *device)
{
    struct stat st;

    // Verify existence of device.
    if (stat(device->name, &st) == -1) {
	fprintf(stderr, "Failed to find device. %d: %s\n",
		errno, strerror(errno));
	return (-1);
    }

    // Verify device is a character device.
    if (!S_ISCHR(st.st_mode)) {
	fprintf(stderr, "%s is not a character device.\n", device->name);
	return (-1);
    }

    // Open device in read/write non-blocking mode.
    if ((device->fd = open(device->name, O_RDWR | O_NONBLOCK)) == -1) {
	fprintf(stderr, "Failed to open device. %d: %s\n",
		errno, strerror(errno));
	goto error;
    }

    // Verify device can stream video.
    if (ch_validate_device(device->fd) == -1)
	goto error;

    return (0);

error:
    device->fd = 0;
    return (-1);
}

int
ch_close_device(struct ch_device *device)
{
    if (device->stream)
	ch_stop_stream(device);

    if (device->in_buffers)
	ch_unmap_buffers(device);

    free(device->in_buffers);
    device->in_buffers = NULL;

    if (device->fd > 0) {
	if (close(device->fd) == -1) {
	    fprintf(stderr, "Failed to close device. %d: %s\n",
		    errno, strerror(errno));
	    return (-1);
	}

	device->fd = 0;
    }

    return (0);
}

struct ch_fmts *
ch_enum_fmts(struct ch_device *device)
{
    // Find maximum format index.
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ch_ioctl(device->fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
	fmtdesc.index++;

    // Create storage array and fill.
    struct ch_fmts *fmts = ch_calloc(1, sizeof(struct ch_fmts));
    if (fmts == NULL)
	return (NULL);

    fmts->length = fmtdesc.index;

    fmts->fmts = ch_calloc(fmtdesc.index, sizeof(uint32_t));
    if (fmts->fmts == NULL) {
	free(fmts);
	return (NULL);
    }

    fmtdesc.index = 0;
    while (ch_ioctl(device->fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
	fmts->fmts[fmtdesc.index++] = fmtdesc.pixelformat;

    return (fmts);
}

void
ch_destroy_fmts(struct ch_fmts *fmts)
{
    free(fmts->fmts);
    free(fmts);
}

struct ch_frmsizes *
ch_enum_frmsizes(struct ch_device *device)
{
    // Find maximum size index.
    struct v4l2_frmsizeenum frmsize;

    frmsize.index = 0;
    frmsize.pixel_format = device->pixelformat;

    while (ch_ioctl(device->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
	frmsize.index++;

    // Only supporting discrete resolutions. Should return on first if not.
    if (frmsize.type != V4L2_FRMSIZE_TYPE_DISCRETE)
	return (NULL);

    // Create storage array and fill.
    struct ch_frmsizes *frmsizes = ch_calloc(1, sizeof(struct ch_frmsizes));
    if (frmsizes == NULL)
	return (NULL);

    frmsizes->length = frmsize.index;

    frmsizes->frmsizes = ch_calloc(frmsize.index, sizeof(struct ch_rect));
    if (frmsizes->frmsizes == NULL) {
	free(frmsizes);
	return (NULL);
    }

    frmsize.index = 0;
    while (ch_ioctl(device->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0)
	frmsizes->frmsizes[frmsize.index++] = (struct ch_rect) {
	    frmsize.discrete.width,
	    frmsize.discrete.height
	};

    return (frmsizes);
}

void
ch_destroy_frmsizes(struct ch_frmsizes *frmsizes)
{
    free(frmsizes->frmsizes);
    free(frmsizes);
}

/**
 * @brief Validates a device's requested format.
 *
 * @param device The device to validate.
 * @return 0 on success, -1 on failure.
 */
static int
ch_validate_fmt(struct ch_device *device)
{
    // Validate requested format and resolution before setting.
    struct ch_fmts *fmts = ch_enum_fmts(device);
    if (fmts == NULL)
	return (-1);

    size_t idx;
    for (idx = 0; idx < fmts->length; idx++)
	if (fmts->fmts[idx] == device->pixelformat)
	    break;

    if (idx == fmts->length) {
	char pixfmt_buf[5];
	ch_pixfmt_to_string(device->pixelformat, pixfmt_buf);

	fprintf(stderr, "Format %s is unsupported by device.\n",
		pixfmt_buf);
	return (-1);
    }

    ch_destroy_fmts(fmts);

    return (0);
}

/**
 * @brief Validates a device's requested framesize.
 *
 * @param device The device to validate.
 * @return 0 on success, -1 on failure.
 */
static int
ch_validate_frmsize(struct ch_device *device)
{
    struct ch_frmsizes *frmsizes = ch_enum_frmsizes(device);

    size_t idx;
    for (idx = 0; idx < frmsizes->length; idx++)
	if (frmsizes->frmsizes[idx].width == device->framesize.width
	    && frmsizes->frmsizes[idx].height == device->framesize.height)
	    break;

    if (idx == frmsizes->length) {
	char pixfmt_buf[5];
	ch_pixfmt_to_string(device->pixelformat, pixfmt_buf);

	fprintf(stderr, "Framesize %ux%u is not supported for format %s.\n",
		device->framesize.width, device->framesize.height,
		pixfmt_buf);
	return (-1);
    }

    ch_destroy_frmsizes(frmsizes);

    return (0);
}

int
ch_set_fmt(struct ch_device *device)
{
    // Validate request.
    if (ch_validate_fmt(device) == -1)
	return (-1);

    if (ch_validate_frmsize(device) == -1)
	return (-1);

    // Set format and framesize on device.
    struct v4l2_format fmt;

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = device->framesize.width;
    fmt.fmt.pix.height = device->framesize.height;
    fmt.fmt.pix.pixelformat = device->pixelformat;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = 0;

    if (ch_ioctl(device->fd, VIDIOC_S_FMT, &fmt) == -1) {
	fprintf(stderr, "Could not set output format.\n");
	return (-1);
    }

    return (0);
}

int
ch_init_stream(struct ch_device *device)
{
    struct v4l2_requestbuffers req;

    req.count = device->num_buffers;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    // Request a number of buffers.
    if (ch_ioctl(device->fd, VIDIOC_REQBUFS, &req) == -1) {
	fprintf(stderr, "Failed to request buffers.\n");
	return (-1);
    }

    // Compare return to requested amount of buffers.
    if (req.count != device->num_buffers) {
	fprintf(stderr, "Insufficient buffer memory on device (%u vs. %u).\n",
		device->num_buffers, req.count);
	return (-1);
    }

    // Allocate buffers.
    device->in_buffers = ch_calloc(req.count, sizeof(struct ch_frmbuf));
    if (device->in_buffers == NULL)
	return (-1);

    // Query each buffer and map it into our address space.
    size_t idx;
    for (idx = 0; idx < req.count; idx++) {
	struct v4l2_buffer buf;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = idx;

	if (ch_ioctl(device->fd, VIDIOC_QUERYBUF, &buf) == -1) {
	    fprintf(stderr, "Failed to query buffers.\n");
	    goto error;
	}

	device->in_buffers[idx].length = buf.length;
	device->in_buffers[idx].start = mmap(
	    NULL,
	    buf.length,
	    PROT_READ | PROT_WRITE,
	    MAP_SHARED,
	    device->fd,
	    buf.m.offset
	);

	if (device->in_buffers[idx].start == MAP_FAILED) {
	    fprintf(stderr, "Failed to mmap buffer. %d: %s\n",
		    errno, strerror(errno));
	    goto error;
	}
    }

    return (0);

error:
    ch_unmap_buffers(device);
    free(device->in_buffers);

    return (-1);
}

int
ch_start_stream(struct ch_device *device)
{
    // Query each buffer to be filled.
    size_t idx;
    for (idx = 0; idx < device->num_buffers; idx++) {
	struct v4l2_buffer buf;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = idx;

	if (ch_ioctl(device->fd, VIDIOC_QBUF, &buf) == -1) {
	    fprintf(stderr, "Failed to request buffer.\n");
	    return (-1);
	}
    }

    // Start streaming.
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ch_ioctl(device->fd, VIDIOC_STREAMON, &type) == -1) {
	fprintf(stderr, "Failed to start stream.\n");
	return (-1);
    }

    device->stream = true;
    return (0);
}

int
ch_stop_stream(struct ch_device *device)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ch_ioctl(device->fd, VIDIOC_STREAMOFF, &type) == -1) {
	fprintf(stderr, "Failed to stop stream.\n");
	return (-1);
    }

    device->stream = false;
    return (0);
}

int
ch_stream(struct ch_device *device, uint32_t num_frames,
	  void (*callback)(struct ch_frmbuf *frm))
{
    if (!device->stream)
	ch_start_stream(device);

    size_t n;
    for (n = 0; (num_frames != 0) ? n < num_frames : 1; n++) {
	// Wait on select for a new frame.
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(device->fd, &fds);

	struct timeval temp = device->timeout;
	int r = select(device->fd + 1, &fds, NULL, NULL, &temp);

	if (r == -1) {
	    fprintf(stderr, "Error on select. %d: %s\n",
		    errno, strerror(errno));
	    return (-1);

	} else if (r == 0) {
	    fprintf(stderr, "Timeout on select.\n");
	    return (-1);
	}

	// Dequeue buffer.
	struct v4l2_buffer buf;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ch_ioctl(device->fd, VIDIOC_DQBUF, &buf) == -1) {
	    fprintf(stderr, "Failure dequeuing buffer.\n");
	    return (-1);
	}

	// Verify buffer is valid.
	if (buf.index > device->num_buffers) {
	    fprintf(stderr, "Bad buffer index returned from deque.\n");
	    return (-1);
	}

	fprintf(stdout, ".");
	fflush(stdout);

	// Requeue buffer.
	if (ch_ioctl(device->fd, VIDIOC_QBUF, &buf) == -1) {
	    fprintf(stderr, "Failed requeuing buffer.\n");
	    return (-1);
	}
    }

    return (0);
}
