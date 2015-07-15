#ifndef CHIASM_H_
#define CHIASM_H_

#include <stdio.h>

#include <linux/types.h>
#include <linux/videodev2.h>

__u32 string_to_pixfmt(char *buf);
void pixfmt_to_string(__u32 pixfmt, char *buf);
void print_v4l2_capability(struct v4l2_capability *caps);
void print_v4l2_fmtdesc(struct v4l2_fmtdesc *fmtdesc);
void print_v4l2_frmsizeenum(struct v4l2_frmsizeenum *frmsize);
void print_v4l2_frmivalenum(struct v4l2_frmivalenum *frmival);

#endif
