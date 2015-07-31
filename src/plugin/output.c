#include <stdlib.h>
#include <stdio.h>

#include <chiasm.h>

int
CH_DL_INIT(struct ch_device *device, struct ch_dl_cx *cx)
{
    device = (struct ch_device *) device;
    cx = (struct ch_dl_cx *) cx;

    fprintf(stderr, "Initializing plugin.\n");
    return (0);
}

int
CH_DL_CALL(struct ch_frmbuf *out)
{
    out = (struct ch_frmbuf *) out;

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
