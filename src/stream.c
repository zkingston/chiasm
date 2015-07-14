#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
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
query_fmt(int fd)
{
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int r;
    while (ioctl_r(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
	if (verbose)
	    print_v4l2_fmtdesc(&fmtdesc);

	struct v4l2_frmsizeenum frmsize;

	frmsize.index = 0;
	frmsize.pixel_format = fmtdesc.pixelformat;
	while (ioctl_r(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0) {
	    if (verbose)
		print_v4l2_frmsizeenum(&frmsize);

	    if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
		struct v4l2_frmivalenum frmival;

		frmival.index = 0;
		frmival.pixel_format = fmtdesc.pixelformat;
		frmival.width = frmsize.discrete.width;
		frmival.height = frmsize.discrete.height;

		while (ioctl_r(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
		    if (verbose)
			print_v4l2_frmivalenum(&frmival);

		    frmival.index++;
		}
	    }

	    frmsize.index++;
	}

	fmtdesc.index++;
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
main(int argc, char *argv[])
{
    int fd;
    char *video_device = CH_DEFAULT_DEVICE;

    int opt;
    char *opts = "d:vh?";
    while ((opt = getopt(argc, argv, opts)) != -1) {
	switch (opt) {
	case 'd':
	    video_device = optarg;
	    break;
	case 'v':
	    verbose = true;
	    break;
	case 'h':
	case '?':
	default:
	    printf(
		"Usage: %s [%s]\n"
		"Stream video device to ach channel.\n"
		"Options:\n"
		"  -d    Specify device name. \"%s\" by default.\n"
		"  -l    Enable verbose output.\n"
		"  -?,h  Show this help.\n",
		argv[0],
		opts,
		CH_DEFAULT_DEVICE
	    );
	    break;
	}
    }

    if ((fd = open_device(video_device)) == -1) {
	fprintf(stderr, "Exiting...\n");
	return (-1);
    }

    if (query_caps(fd) == -1) {
	fprintf(stderr, "Exiting...\n");
	return (-1);
    }

    if (query_fmt(fd) == -1) {
	fprintf(stderr, "Exiting...\n");
	return (-1);
    }

    return (0);
}
