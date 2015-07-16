#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>

#include <chiasm.h>

static int
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

static void *
ch_calloc(size_t nmemb, size_t size)
{
    void *r = calloc(nmemb, size);

    if (r == NULL || errno == ENOMEM) {
	fprintf(stderr, "No memory available.\n");
	return (NULL);
    }

    return (r);
}

static void
ch_pixfmt_to_string(uint32_t pixfmt, char *buf)
{
    size_t idx;
    for (idx = 0; idx < 4; idx++)
	buf[idx] = (pixfmt >> (8 * idx)) & 0xFF;

    buf[idx] = '\0';
}

static uint32_t
ch_string_to_pixfmt(char *buf)
{
    uint32_t pixfmt = 0;

    size_t idx;
    for (idx = 0; idx < 4 && buf[idx] != '\0'; idx++)
	pixfmt |= (buf[idx] << (8 * idx));

    return (pixfmt);
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
    if (close(device->fd) == -1) {
	fprintf(stderr, "Failed to close device. %d: %s\n",
		errno, strerror(errno));
	return (-1);
    }

    device->fd = 0;
    return (0);
}

struct ch_fmts *
ch_enum_formats(struct ch_device *device)
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


}
