#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

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

    ch_destroy_ctrls(ctrls);
    return (0);
}

int
main(int argc, char *argv[])
{
    size_t n_frames = CH_DEFAULT_NUMFRAMES;
    bool list = false;
    bool ctrl = false;

    ch_init_device(&device);

    int opt;
    while ((opt = getopt(argc, argv, CH_OPTS "cn:lh?")) != -1) {
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
            ctrl = true;
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
                CH_HELP
                " -n   Number of frames to read. 0 = Infinite. %d by default.\n"
                " -l   List formats, resolutions, framerates and exit.\n"
                " -?,h Show this help.\n",
                argv[0],
                CH_DEFAULT_NUMFRAMES
            );

            return (0);
        }
    }

    // Install signal handlers to clean up and exit nicely.
    signal(SIGINT, signal_handler);

    int r = 0;
    if ((r = ch_open_device(&device)) == -1)
        goto cleanup;

    if (list)
        if (list_formats(&device) == -1)
            goto cleanup;

    if (ctrl)
        if (list_ctrls(&device) == -1)
            goto cleanup;

    if (list || ctrl)
        goto cleanup;

    if ((r = ch_set_fmt(&device)) == -1)
        goto cleanup;

    if ((r = ch_stream_async(&device, n_frames, stream_callback)) == -1)
        goto cleanup;

    sleep(3);

    ch_stream_async_join(&device);

cleanup:
    ch_stop_stream(&device);
    ch_close_device(&device);

    return (r);
}
