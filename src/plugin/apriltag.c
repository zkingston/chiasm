#include <stdlib.h>
#include <stdio.h>

// Apriltag includes.
#include <apriltag.h>
#include <tag36h11.h>
#include <common/zarray.h>
#include <common/image_u8.h>

#include <chiasm.h>

apriltag_family_t *tag_family = NULL;
apriltag_detector_t *tag_detector = NULL;

int
CH_DL_INIT(struct ch_device *device)
{
    device = (struct ch_device *) device;

    // Initialize AprilTag tag family.
    tag_family = tag36h11_create();

    tag_family->black_border = 1;

    // Initialize AprilTag detector.
    tag_detector = apriltag_detector_create();
    apriltag_detector_add_family(tag_detector, tag_family);

    tag_detector->quad_decimate = 1.0;
    tag_detector->quad_sigma = 0.0;
    tag_detector->nthreads = 64;
    tag_detector->debug = 0;

    tag_detector->refine_edges = 0;
    tag_detector->refine_decode = 0;
    tag_detector->refine_pose = 0;

    return (0);
}

int
CH_DL_CALL(struct ch_device *device)
{
    image_u8_t image = {
        .width = device->framesize.width,
        .height = device->framesize.height,
        .stride = device->out_stride,
        .buf = device->out_buffer.start
    };

    zarray_t *detections = apriltag_detector_detect(tag_detector, &image);

    int i;
    for (i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t *det;
        zarray_get(detections, i, &det);

        size_t x;
        for (x = 0; x < 4; x++)
            image_u8_draw_line(&image,
                               det->p[x][0], det->p[x][1],
                               det->p[(x + 1) % 4][0], det->p[(x + 1) % 4][1],
                               255, 3);
    }

    apriltag_detections_destroy(detections);

    return (0);
}

int
CH_DL_QUIT(struct ch_device *device)
{
    device = (struct ch_device *) device;

    apriltag_detector_destroy(tag_detector);
    tag36h11_destroy(tag_family);

    tag_detector = NULL;
    tag_family = NULL;

    return (0);
}