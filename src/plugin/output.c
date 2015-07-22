#include <stdlib.h>
#include <stdio.h>

#include <chiasm.h>

int
CH_DL_CALL(struct ch_device *device)
{
    device = (struct ch_device *) device;

    fprintf(stderr, ".");
    fflush(stderr);

    return (0);
}
