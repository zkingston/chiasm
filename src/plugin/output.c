#include <stdlib.h>
#include <stdio.h>

#include <chiasm.h>

// Basic example of a plugin.

int
CH_DL_INIT(struct ch_device *device)
{
    device = (struct ch_device *) device;

    return (0);
}

int
CH_DL_CALL(struct ch_device *device)
{
    device = (struct ch_device *) device;

    fprintf(stderr, ".");
    fflush(stderr);

    return (0);
}
