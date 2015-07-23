#include <stdlib.h>
#include <stdio.h>

#include <chiasm.h>

int
CH_DL_CALL(struct ch_device *device)
{
    fwrite(device->out_buffer.start, device->out_buffer.length, 1, stdout);
    fflush(stdout);

    fprintf(stderr, ".");
    fflush(stderr);

    return (0);
}
