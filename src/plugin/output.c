#include <stdlib.h>
#include <stdio.h>

#include <chiasm.h>

int
CH_DL_INIT(struct ch_device *device, struct ch_dl_cx *cx)
{
    fprintf(stderr, "Initializing plugin.\n");
    return (0);
}

int
CH_DL_CALL(struct ch_frmbuf *out)
{
    // fwrite(device->out_buffer.start, device->out_buffer.length, 1, stdout);
    // fflush(stdout);

    fprintf(stderr, ".");
    fflush(stderr);

    return (0);
}

int
CH_DL_QUIT(void)
{
    fprintf(stderr, "Destroying plugin.\n");
    fflush(stderr);

    return (0);
}
