#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <linux/videodev2.h>
#include <chiasm.h>

struct ch_device device;

/**
 * @brief Signal handler to gracefully shutdown in the case of an interrupt.
 *
 * @param signal The signal sent.
 * @return None.
 */
void
signal_handler(int signal)
{
    fprintf(stderr, "Signal %s received. Cleaning up and exiting...\n",
            strsignal(signal));

    ch_stop_stream(&device);
    ch_close_device(&device);

    exit(0);
}

/**
 * @brief Callback function on new frame. Writes frame out to stdout.
 *
 * @param frm New image frame from camera.
 * @return Always 0.
 */
static int
stream_callback(struct ch_frmbuf *frm)
{
//     fwrite(frm->start, frm->length, 1, stdout);
//     fflush(stdout);

    fprintf(stderr, ".");
    fflush(stderr);

    return (0);
}

/**
 * @brief List the available formats and their resolutions for a device.
 *
 * @param device Device to list formats for.
 * @return 0 on success, -1 on failure.
 */
static int
list_formats(struct ch_device *device)
{
    // Get all formats
    struct ch_fmts *fmts = ch_enum_fmts(device);
    if (fmts == NULL)
        return (-1);

    // Iterate over all formats and then their framesizes.
    size_t idx;
    for (idx = 0; idx < fmts->length; idx++) {
        char pixfmt_buf[5];
        ch_pixfmt_to_string(fmts->fmts[idx], pixfmt_buf);

        printf("%s:", pixfmt_buf);

        device->pixelformat = fmts->fmts[idx];
        struct ch_frmsizes *frmsizes = ch_enum_frmsizes(device);
        if (frmsizes == NULL)
            break;

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

    // Return 0 if we didn't break out early.
    return ((idx == fmts->length) ? 0 : -1);
}

/**
 * @brief List the available user controls on a device.
 *
 * @param device Device to list the controls for.
 * @return 0 on success, -1 on failure.
 */
static int
list_ctrls(struct ch_device *device)
{
    struct ch_ctrls *ctrls = ch_enum_ctrls(device);
    if (ctrls == NULL)
	return (-1);

    size_t idx;
    for (idx = 0; idx < ctrls->length; idx++)
	printf("%s\n", ctrls->ctrls[idx].name);

    ch_destroy_ctrls(ctrls);
    return (0);
}

/**
 * @brief List detailed information about a control on a device.
 *
 * @param device The device to list the control information from.
 * @param control Name of control to detail.
 * @return 0 on success, -1 on failure.
 */
static int
ctrl_info(struct ch_device *device, const char *ctrl_name)
{
    struct ch_ctrls *ctrls = ch_enum_ctrls(device);
    if (ctrls == NULL)
	return (-1);

    size_t l = strlen(ctrl_name);

    int r = -1;
    size_t idx;
    for (idx = 0; idx < ctrls->length; idx++) {
	struct ch_ctrl *ctrl = &ctrls->ctrls[idx];

	if (strncmp(ctrl_name, ctrl->name, l) == 0 && l == strlen(ctrl->name)) {
	    printf("Information for control \"%s\"\n", ctrl_name);

	    switch (ctrl->type) {
	    case V4L2_CTRL_TYPE_INTEGER:
		printf("   Type: Integer\n");
		printf("Default: %d\n", ctrl->defval / ctrl->step);
		printf("  Range: %d / %d\n",
		       ctrl->min / ctrl->step,
		       ctrl->max / ctrl->step);

		break;

	    case V4L2_CTRL_TYPE_BOOLEAN:
		printf("   Type: Boolean\n");
		printf("Default: %d\n", ctrl->defval);
		break;

	    case V4L2_CTRL_TYPE_MENU: {
		struct ch_ctrl_menu *menu = ch_enum_ctrl_menu(device, ctrl);
		if (menu == NULL)
		    goto exit;

		printf("   Type: Menu\n");
		printf("Default: %s\n", menu->items[ctrl->defval].name);
		printf("Options: ");

		size_t jdx;
		for (jdx = 0; jdx < menu->length; jdx++)
		    if (menu->items[jdx].name[0] != '\0')
			printf("%s%s", menu->items[jdx].name,
			       (jdx != menu->length - 1) ? ", " : "\n");

		ch_destroy_ctrl_menu(menu);
		break;
	    }
	    default:
		printf("   Type: Unsupported\n");
		break;
	    }

	    r = 0;
	    break;
	}
    }

    if (r == -1)
	printf("Control not found.\n");

exit:
    ch_destroy_ctrls(ctrls);
    return (r);
}

int
main(int argc, char *argv[])
{
    size_t n_frames = CH_DEFAULT_NUMFRAMES;
    bool list = false;
    bool lctrls = false;
    char *ctrl = NULL;

    ch_init_device(&device);

    int opt;
    while ((opt = getopt(argc, argv, CH_OPTS "i:cn:lh?")) != -1) {
        switch (opt) {
        case 'd':
        case 't':
        case 'b':
        case 'f':
        case 'g':
            if (ch_parse_device_opt(opt, optarg, &device) == -1)
                return (-1);

            break;

        case 'l':
            list = true;
            break;

        case 'c':
            lctrls = true;
            break;

	case 'i':
	    if (ctrl == NULL)
		ctrl = optarg;
	    else {
		fprintf(stderr, "Argument for -i already given.\n");
		return (-1);
	    }

	    break;

        case 'n':
            n_frames = strtoul(optarg, NULL, 10);
            if (errno == EINVAL || errno == ERANGE) {
                fprintf(stderr, "Invalid number of frames %s.\n", optarg);
                return (-1);
            }

            break;

        case 'h':
        case '?':
        default:
            printf(
                "Usage: %s [OPTIONS]\n"
                "Options:\n"
                CH_HELP_D
		CH_HELP_F
		CH_HELP_G
		CH_HELP_B
		CH_HELP_T
                " -n   Number of frames to read. 0 = Infinite. %d by default.\n"
                " -l   List formats, resolutions, framerates and exit.\n"
		" -c   List supported controls and exit.\n"
		" -i   List information about a control. Requires argument.\n"
                " -?,h Show this help.\n",
                argv[0],
                CH_DEFAULT_NUMFRAMES
            );

            return (0);
        }
    }

    // Install signal handlers to clean up and exit nicely.
    signal(SIGINT, signal_handler);

    // Enable error output to stderr.
    ch_set_stderr(true);

    int r = 0;
    if ((r = ch_open_device(&device)) == -1)
        goto cleanup;

    if (list)
        if ((r = list_formats(&device)) == -1)
            goto cleanup;

    if (lctrls)
        if ((r = list_ctrls(&device)) == -1)
            goto cleanup;

    if (ctrl)
	if ((r = ctrl_info(&device, ctrl)) == -1)
	    goto cleanup;

    if (list || lctrls || ctrl != NULL)
        goto cleanup;

    if ((r = ch_set_fmt(&device)) == -1)
        goto cleanup;

    if ((r = ch_stream(&device, n_frames, stream_callback)) == -1)
        goto cleanup;

cleanup:
    ch_stop_stream(&device);
    ch_close_device(&device);

    return (r);
}
