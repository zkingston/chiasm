#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// Apriltag includes.
#include <apriltag.h>
#include <tag36h11.h>
#include <common/zarray.h>
#include <common/image_u8.h>
#include <common/homography.h>

#include <chiasm.h>

apriltag_family_t *tag_family = NULL;
apriltag_detector_t *tag_detector = NULL;

uint32_t width, height, stride;

bool calib = false;
double f[2];
double c[2];

int
CH_DL_INIT(struct ch_device *device, struct ch_dl_cx *cx)
{
    width = device->framesize.width;
    height = device->framesize.height;

    if (device->calib) {
        calib = true;
        cx->undistort = true;
        f[0] = device->calib->camera_mat[0][0];
        f[1] = device->calib->camera_mat[1][1];

        c[0] = device->calib->camera_mat[0][2];
        c[1] = device->calib->camera_mat[1][2];
    }

    cx->out_pixfmt = AV_PIX_FMT_GRAY8;
    stride = cx->out_stride = ch_calc_stride(cx, width, 96);

    // Initialize AprilTag tag family.
    tag_family = tag36h11_create();

    tag_family->black_border = 1;

    // Initialize AprilTag detector.
    tag_detector = apriltag_detector_create();
    apriltag_detector_add_family(tag_detector, tag_family);

    tag_detector->quad_decimate = 3;
    tag_detector->quad_sigma = 0.0;
    tag_detector->nthreads = 2;
    tag_detector->debug = 0;

    tag_detector->refine_edges = 1;
    tag_detector->refine_decode = 0;
    tag_detector->refine_pose = 0;

    return (0);
}

int
CH_DL_CALL(struct ch_frmbuf *in_buf)
{
    image_u8_t image = {
        .width = width,
        .height = height,
        .stride = stride,
        .buf = in_buf->start
    };

    zarray_t *detections = apriltag_detector_detect(tag_detector, &image);

    if (!zarray_size(detections))
        return (0);

    int i;
    for (i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t *det;
        zarray_get(detections, i, &det);

        if (calib) {
            matd_t *pose = homography_to_pose(det->H,
                                              f[0], f[1], c[0], c[1]);

            double x = matd_get(pose, 0, 3) * 20.5;
            double y = matd_get(pose, 1, 3) * 20.5;
            double z = matd_get(pose, 2, 3) * 20.5;

            fprintf(stderr, "Tag %d - (%5.2f, %5.2f, %5.2f)\n", det->id, x, y, z);
        }
    }

    fprintf(stderr, "\n");

    apriltag_detections_destroy(detections);

    return (0);
}

int
CH_DL_QUIT(void)
{
    apriltag_detector_destroy(tag_detector);
    tag36h11_destroy(tag_family);

    tag_detector = NULL;
    tag_family = NULL;

    return (0);
}
