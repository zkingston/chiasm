#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <linux/videodev2.h>
#include <chiasm.h>

#define MAX_PLUGINS 10

struct ch_device device;
struct ch_dl *plugins[MAX_PLUGINS];
size_t plugin_max = 0;

/**
 * @brief Signal handler to gracefully shutdown in the case of an interrupt.
 *
 * @param signal The signal sent.
 * @return None.
 */
void
signal_handler(int signal)
{
    fprintf(stderr, "\nSignal %s received. Cleaning up and exiting...\n",
            strsignal(signal));

    device.stream = false;
}

/**
 * @brief List the available formats and their resolutions for a device.
 *
 * @param device Device to list formats for.
 * @return 0 on success, -1 on failure.
 */
static int
list_formats(void)
{
    // Get all formats
    struct ch_fmts *fmts = ch_enum_fmts(&device);
    if (fmts == NULL)
        return (-1);

    // Iterate over all formats and then their framesizes.
    size_t idx;
    for (idx = 0; idx < fmts->length; idx++) {
        char pixfmt_buf[5];
        ch_pixfmt_to_string(fmts->fmts[idx], pixfmt_buf);

        printf("%4s:", pixfmt_buf);

        device.in_pixfmt = fmts->fmts[idx];
        struct ch_frmsizes *frmsizes = ch_enum_frmsizes(&device);
        if (frmsizes == NULL)
            break;

        size_t jdx;
        for (jdx = 0; jdx < frmsizes->length; jdx++) {
            device.framesize.width = frmsizes->frmsizes[jdx].width;
            device.framesize.height = frmsizes->frmsizes[jdx].height;

            printf(" %4ux%4u (%4.1f fps)",
                   device.framesize.width, device.framesize.height,
                   ch_get_fps(&device));

            if ((jdx + 1) % 3 == 0)
                printf("\n     ");

        }

        ch_destroy_frmsizes(frmsizes);
        printf("\n\n");
    }

    ch_destroy_fmts(fmts);
    fflush(stdout);

    // Return 0 if we didn't break out early.
    return ((idx == fmts->length) ? 0 : -1);
}

int
main(int argc, char *argv[])
{
    bool list = false;
    char *calibration_file = NULL;

    // Enable error output to stderr.
    ch_set_stderr(true);

    ch_init_device(&device);

    int opt;
    while ((opt = getopt(argc, argv, CH_OPTS "c:i:n:lh?")) != -1) {
        switch (opt) {
        case 'd':
        case 't':
        case 'b':
        case 'f':
        case 'g':
            if (ch_parse_device_opt(opt, optarg, &device) == -1)
                return (-1);

            break;

        case 'c':
            calibration_file = optarg;
            break;

        case 'l':
            list = true;
            break;

	case 'i':
	    plugins[plugin_max] = ch_dl_load(optarg);
	    if (plugins[plugin_max] == NULL)
		return (-1);

	    plugin_max++;
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
		" -i   Filename of chiasm plugin to load. Required.\n"
                " -l   List formats, resolutions, framerates and exit.\n"
                " -?,h Show this help.\n",
                argv[0]
            );

            return (0);
        }
    }

    // Install signal handlers to clean up and exit nicely.
    signal(SIGINT, signal_handler);

    int r = 0;
    if ((r = ch_open_device(&device)) == -1)
        goto cleanup;

    if (list) {
        r = list_formats();
        goto cleanup;
    }

    if ((r = ch_set_fmt(&device)) == -1)
        goto cleanup;

    if (calibration_file)
        if (ch_load_calibration(&device, calibration_file) == -1)
            goto cleanup;

    r = ch_stream(&device, plugins, plugin_max);

cleanup:
    ch_close_calibration(&device);
    ch_close_device(&device);

    size_t idx;
    for (idx = 0; idx < plugin_max; idx++)
        ch_dl_close(plugins[idx]);

    return (r);
}
