#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/videodev2.h>

#define CH_DEFAULT_DEVICE "/dev/video0"

bool verbose = false;
bool list = false;

/**
 * Print out contents of a struct v4l2_capability.
 */
void
print_v4l2_capability(struct v4l2_capability *caps)
{
    printf(
	"Driver/Hardware Capabilities:\n"
	"      Driver: %s\n"
	"        Card: %s\n"
	"    Bus Info: %s\n"
	"     Version: %u.%u.%u\n"
	"  Cap. Flags: %08X\n",
	caps->driver,
	caps->card,
	caps->bus_info,
	(caps->version >> 16) & 0xFF,
	(caps->version >> 8) & 0xFF,
	caps->version & 0xFF,
	caps->capabilities
    );

    // caps->device_caps will only be set if the following test is true.
    if (caps->capabilities & V4L2_CAP_DEVICE_CAPS)
	printf("  Dev. Flags: %08X\n\n", caps->device_caps);
    else
	printf("\n");
}

/**
 * Print out contents of a struct v4l2_fmtdesc.
 */
void
print_v4l2_fmtdesc(struct v4l2_fmtdesc *fmtdesc)
{
    printf(
	"Format %d: (%c%c) %s - %c%c%c%c\n",
	fmtdesc->index,
	(fmtdesc->flags & V4L2_FMT_FLAG_COMPRESSED) ? 'C' : '-',
	(fmtdesc->flags & V4L2_FMT_FLAG_EMULATED) ? 'E' : '-',
	fmtdesc->description,
	fmtdesc->pixelformat & 0xFF,
	(fmtdesc->pixelformat >> 8) & 0xFF,
	(fmtdesc->pixelformat >> 16) & 0xFF,
	(fmtdesc->pixelformat >> 24) & 0xFF
    );
}

/**
 * Print out contents of a struct v4l2_frmsizeenum.
 */
void
print_v4l2_frmsizeenum(struct v4l2_frmsizeenum *frmsize)
{
    // Only support for discrete resolutions currently.
    if (frmsize->type != V4L2_FRMSIZE_TYPE_DISCRETE)
	printf("Unsupported framesize type.\n");


    printf(
	"  Framesize %d: %d x %d\n",
	frmsize->index,
	frmsize->discrete.width,
	frmsize->discrete.height
    );
}

/**
 * Print out contents of a struct v4l2_frmivalenum.
 */
void
print_v4l2_frmivalenum(struct v4l2_frmivalenum *frmival)
{
    // Only support for discrete framerates currently.
    if (frmival->type != V4L2_FRMIVAL_TYPE_DISCRETE)
	printf("Unsupported framerate type.\n");

    printf(
	"    Framerate %d: %d / %d\n",
	frmival->index,
	frmival->discrete.numerator,
	frmival->discrete.denominator
    );
}

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
list_fmts(int fd)
{
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (ioctl_r(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
	print_v4l2_fmtdesc(&fmtdesc);

	struct v4l2_frmsizeenum frmsize;

	frmsize.index = 0;
	frmsize.pixel_format = fmtdesc.pixelformat;

	while (ioctl_r(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
	    print_v4l2_frmsizeenum(&frmsize);

	    // Only supporting discrete resolutions currently.
	    if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		struct v4l2_frmivalenum frmival;

		// Print out maximum framerate for each resolution.
		frmival.index = 0;
		frmival.pixel_format = fmtdesc.pixelformat;
		frmival.width = frmsize.discrete.width;
		frmival.height = frmsize.discrete.height;

		if (ioctl_r(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) != -1)
		    print_v4l2_frmivalenum(&frmival);
	    }

	    frmsize.index++;
	}

	fmtdesc.index++;
    }

    return (0);
}

/**
 * Initialize device state.
 */
int
init_device(int fd, int format_idx, int resolution_idx)
{
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_frmsizeenum frmsize;

    fmtdesc.index = format_idx;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl_r(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
	fprintf(stderr, "Invalid format index %d.\n", format_idx);
	return (-1);
    }

    frmsize.index = resolution_idx;
    frmsize.pixel_format = fmtdesc.pixelformat;

    if (ioctl_r(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == -1) {
	fprintf(stderr, "Invalid framesize index %d.\n", resolution_idx);
	return (-1);
    }

    struct v4l2_format format;

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = frmsize.discrete.width;
    format.fmt.pix.height = frmsize.discrete.height;
    format.fmt.pix.pixelformat = fmtdesc.pixelformat;
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

    int format_idx = -1;
    int resolution_idx = -1;

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
	    format_idx = parse_uint(optarg);
	    if (format_idx < 0)
		return (-1);

	    break;
	case 'r':
	    resolution_idx = parse_uint(optarg);
	    if (resolution_idx < 0)
		return (-1);

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

    if (list) {
	list_fmts(fd);
	goto cleanup;
    }

    if (format_idx == -1 || resolution_idx == -1) {
	fprintf(stderr, "Must specify output format and resolution.\n");
	goto cleanup;
    }

    if (init_device(fd, format_idx, resolution_idx) == -1)
	goto cleanup;

cleanup:
    if (fd > 0)
	close_device(fd);

    return (0);
}
