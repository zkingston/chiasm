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
	"Supported Format Description:\n"
	"         Index: %d\n"
	"          Type: %d\n"
	"    Compressed: %c\n"
	"      Emulated: %c\n"
	"   Description: %s\n"
	"  Pixel Format: %c%c%c%c\n\n",
	fmtdesc->index,
	fmtdesc->type,
	(fmtdesc->flags & V4L2_FMT_FLAG_COMPRESSED) ? 'T' : 'F',
	(fmtdesc->flags & V4L2_FMT_FLAG_EMULATED) ? 'T' : 'F',
	fmtdesc->description,
	fmtdesc->pixelformat & 0xFF,
	(fmtdesc->pixelformat >> 8) & 0xFF,
	(fmtdesc->pixelformat >> 16) & 0xFF,
	(fmtdesc->pixelformat >> 24) & 0xFF
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
 * Query the capabilities of a device.
 */
int
query_caps(int fd)
{
    struct v4l2_capability caps;

    if (ioctl_r(fd, VIDIOC_QUERYCAP, &caps) == -1)
	return (-1);

    if (verbose)
	print_v4l2_capability(&caps);

    // Verify single-planar video capture is supported.
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

int
query_fmt(int fd)
{
    struct v4l2_fmtdesc fmtdesc;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int r;
    while ((r = ioctl_r(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0) {
	if (verbose)
	    print_v4l2_fmtdesc(&fmtdesc);

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
