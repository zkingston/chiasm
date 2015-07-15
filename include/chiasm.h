#ifndef CHIASM_H_
#define CHIASM_H_

#include <stdio.h>

#include <linux/types.h>
#include <linux/videodev2.h>

struct ch_buf {
    void *start;
    __u32 length;
};

#endif
