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
#include <fcntl.h>

#include <linux/types.h>
#include <linux/videodev2.h>

#include <chiasm.h>

#define CH_DEFAULT_DEVICE "/dev/video0"

bool verbose = false;
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
 * Query the capabilities of a device, verify support.
 */
int
query_caps(int fd)
{
    struct v4l2_capability caps;

    if (ioctl_r(fd, VIDIOC_QUERYCAP, &caps) == -1)
	return (-1);

    if (verbose)
	print_v4l2_capability(&caps);

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
 * Query available formats for a device.
 */
int
query_fmts(int fd, __u32 pixelformat, __u32 frame_width, __u32 frame_height)
{
    bool valid = false;
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl_r(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
	bool check_fmt = false;
	struct v4l2_frmsizeenum frmsize;
	char pixfmt_buf[5];
	pixfmt_to_string(fmtdesc.pixelformat, pixfmt_buf);

	frmsize.index = 0;
	frmsize.pixel_format = fmtdesc.pixelformat;

	if (list)
	    printf("Format %s: ", pixfmt_buf);
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
			printf(
			    "%ux%u(%u/%u fps), ",
			    frmsize.discrete.width,
			    frmsize.discrete.height,
			    frmival.discrete.numerator,
			    frmival.discrete.denominator
		        );

		} else if (check_fmt) {
		    if (frame_width == frmsize.discrete.width
			&& frame_height == frmsize.discrete.height) {
			valid = true;
			return (0);
		    }
		}
	    }

	    if (valid)
		break;

	    frmsize.index++;
	}

	if (check_fmt && !valid) {
	    fprintf(stderr, "%ux%u is invalid geometry for format %s.\n",
		    frame_width, frame_height, pixfmt_buf);
	    return (-1);
	}

	if (list)
	    printf("\n");

	fmtdesc.index++;
    }

    if (!valid && !list) {
	char in_pixfmt_buf[5];
	pixfmt_to_string(pixelformat, in_pixfmt_buf);
	fprintf(stderr, "Invalid format %s.\n", in_pixfmt_buf);
    }

    return ((list && !valid) ? -1 : 0);
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

    // Open device in read/write nonblocking mode.
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
parse_uint(char *s)
{
    long tmp = strtol(s, NULL, 10);

    if (errno == EINVAL) {
	fprintf(stderr, "Invalid value in %s.\n", s);
	return (-1);
    }

    if (tmp < 0 || tmp > INT_MAX) {
	fprintf(stderr, "Value of %s out of range.\n", s);
	return (-1);
    }

    return ((int) tmp);
}

int
main(int argc, char *argv[])
{
    int fd;
    char *video_device = CH_DEFAULT_DEVICE;
    __u32 pixel_format = string_to_pixfmt("YUYV");
    __u32 frame_width = 320;
    __u32 frame_height = 240;

    int opt;
    char *opts = "d:f:r:s:lvh?";
    while ((opt = getopt(argc, argv, opts)) != -1) {
	switch (opt) {
	case 'd':
	    video_device = optarg;
	    break;
	case 'v':
	    verbose = true;
	    break;
	case 'l':
	    list = true;
	    break;
	case 'f':
	    if (strnlen(optarg, 5) != 4) {
		fprintf(stderr, "Pixel formats must be 4 characters in length.\n");
		return (-1);
	    }

	    pixel_format = string_to_pixfmt(optarg);
	    break;
	case 'r':
	    if (sscanf(optarg, "%ux%u", &frame_width, &frame_height) != 2) {
		fprintf(stderr, "Error parsing geometry string.\n");
		return (-1);
	    }

	    break;
	case 'h':
	case '?':
	default:
	    printf(
		"Usage: %s [%s]\n"
		"Stream video device to ach channel.\n"
		"Options:\n"
		"  -d    Device name. \"%s\" by default.\n"
		"  -f    Index of image format to use.\n"
		"  -r    Index of image resolution to use.\n"
		"  -l    List formats, resolutions, framerates and exit.\n"
		"  -v    Enable verbose output.\n"
		"  -?,h  Show this help.\n",
		argv[0],
		opts,
		CH_DEFAULT_DEVICE
	    );
	    break;
	}
    }

    if ((fd = open_device(video_device)) == -1) {
	return (-1);
    }

    if (query_caps(fd) == -1)
	goto cleanup;

    if (query_fmts(fd, pixel_format, frame_width, frame_height) == -1)
	goto cleanup;

    if (init_device(fd, pixel_format, frame_width, frame_height) == -1)
	goto cleanup;

cleanup:
    if (fd > 0)
	close_device(fd);

    return (0);
}
