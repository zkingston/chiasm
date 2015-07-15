#include <linux/types.h>
#include <linux/videodev2.h>

#include <chiasm.h>

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
    char buf[5];
    pixfmt_to_string(fmtdesc->pixelformat, buf);

    printf(
	"Format %d: (%c%c) %s (%s)\n",
	fmtdesc->index,
	(fmtdesc->flags & V4L2_FMT_FLAG_COMPRESSED) ? 'C' : '-',
	(fmtdesc->flags & V4L2_FMT_FLAG_EMULATED) ? 'E' : '-',
	buf,
	fmtdesc->description
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
	"Framesize %d: %d x %d\n",
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
	"Framerate %d: %d / %d\n",
	frmival->index,
	frmival->discrete.numerator,
	frmival->discrete.denominator
    );
}
