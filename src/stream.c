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

int
stream_callback(struct ch_frmbuf *frm)
{
    fwrite(frm->start, frm->length, 1, stdout);
    fflush(stdout);

    fprintf(stderr, ".");
    fflush(stderr);

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
    size_t num_frames = CH_DEFAULT_NUMFRAMES;
    bool list = false;

    struct ch_device device;
    ch_init_device(&device);

    int opt;
    char *opts = "n:t:b:d:f:g:s:lh?";
    while ((opt = getopt(argc, argv, opts)) != -1) {
	switch (opt) {
	case 'd':
	    device.name = optarg;
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

	    device.timeout = ch_sec_to_timeval(r);
	    break;
	}
	case 'b':
	    device.num_buffers = (uint32_t) strtoul(optarg, NULL, 10);
	    if (errno == EINVAL || errno == ERANGE || device.num_buffers == 0) {
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

	    device.pixelformat = ch_string_to_pixfmt(optarg);
	    break;
	case 'g':
	    if (sscanf(optarg, "%ux%u",
		       &device.framesize.width, &device.framesize.height) != 2) {
		fprintf(stderr, "Error parsing geometry string.\n");
		return (-1);
	    }

	    break;
	case 'h':
	case '?':
	default:
	    printf(
		"Usage: %s [OPTIONS]\n"
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
    if ((r = ch_open_device(&device)) == -1)
     	goto cleanup;

    if (list) {
	list_formats(&device);
	goto cleanup;
    }

    if ((r = ch_set_fmt(&device)) == -1)
	goto cleanup;

    if ((r = ch_init_stream(&device)) == -1)
	goto cleanup;

    if ((r = ch_stream(&device, num_frames, stream_callback)) == -1)
	goto cleanup;

cleanup:
    ch_close_device(&device);

    return (r);
}
