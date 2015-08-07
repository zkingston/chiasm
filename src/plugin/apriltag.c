#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// Apriltag includes.
#include <apriltag.h>
#include <tag36h11.h>
#include <common/zarray.h>
#include <common/image_u8.h>
#include <common/homography.h>

#include <sns.h>
#include <amino.h>

#include <chiasm.h>

#define MAX_TAG_ID 32
#define TAG_SIZE_2 (41.0 / 2.0)

apriltag_family_t *tag_family = NULL;
apriltag_detector_t *tag_detector = NULL;

const char *marker_chan_name = "markers";
ach_channel_t marker_chan;
ach_channel_t *sns_chans[] = { &marker_chan, NULL };

uint32_t width, height, stride;

bool calib = false;
double f[2];
double c[2];

static void
pose_to_q(matd_t *p, double q[4])
{
    double R[9];

    size_t idx;
    for (idx = 0; idx < 3; idx++) {
        size_t jdx;
        for (jdx = 0; jdx < 3; jdx++)
            AA_MATREF(R, 3, idx, jdx) = matd_get(p, idx, jdx);
    }

    aa_tf_rotmat2quat(R, q);
}

int
CH_DL_INIT(struct ch_device *device, struct ch_dl_cx *cx)
{
    // We need device calibration in order to localize tags.
    if (device->calib == NULL)
        return (-1);

    calib = true;
    cx->undistort = true;

    // Grab calibration parameters
    size_t idx;
    for (idx = 0; idx < 2; idx++) {
        f[idx] = device->calib->camera_mat[idx][idx];
        c[idx] = device->calib->camera_mat[idx][2];
    }

    width = device->framesize.width;
    height = device->framesize.height;

    cx->out_pixfmt = AV_PIX_FMT_GRAY8;
    stride = cx->out_stride = ch_calc_stride(cx, width, 96);

    // Initialize AprilTag tag family.
    tag_family = tag36h11_create();

    tag_family->black_border = 1;

    // Initialize AprilTag detector.
    tag_detector = apriltag_detector_create();
    apriltag_detector_add_family(tag_detector, tag_family);

    tag_detector->quad_decimate = width / 320.0;
    tag_detector->quad_sigma = 0.0;
    tag_detector->nthreads = 4;
    tag_detector->debug = 0;

    tag_detector->refine_edges = 1;
    tag_detector->refine_decode = 0;
    tag_detector->refine_pose = 0;

    // Set up channel to publish marker information over.
    sns_init();

    ach_attr_t attr;
    ach_attr_init(&attr);
    ach_attr_set_lock_source(&attr, 1);
    sns_chan_open(&marker_chan, marker_chan_name, NULL);

    sns_sigcancel(sns_chans, sns_sig_term_default);

    sns_start();

    return (0);
}

int
CH_DL_CALL(struct ch_frmbuf *in_buf)
{
    if (sns_cx.shutdown)
        return (-1);

    image_u8_t image = {
        .width = width,
        .height = height,
        .stride = stride,
        .buf = in_buf->start
    };

    zarray_t *detections = apriltag_detector_detect(tag_detector, &image);

    struct timespec now = sns_now();
    struct sns_msg_wt_tf *msg = sns_msg_wt_tf_local_alloc(MAX_TAG_ID);
    sns_msg_set_time(&msg->header, &now, 0);

    int i;
    for (i = 0; i < zarray_size(detections); i++) {
        apriltag_detection_t *det;
        zarray_get(detections, i, &det);

        if (det->id > MAX_TAG_ID) {
            ch_error("Tag ID greater than maximum allowed ID");
            return (-1);
        }

        matd_t *pose = homography_to_pose(det->H,
                                          f[0], f[1], c[0], c[1]);

        double q[4];
        pose_to_q(pose, q);

        sns_wt_tf *wt_tf = &(msg->wt_tf[det->id]);

        size_t idx;
        for (idx = 0; idx < 3; idx++)
            wt_tf->tf.v.data[idx] = matd_get(pose, idx, 3) * TAG_SIZE_2;

        for (idx = 0; idx < 4; idx++)
            wt_tf->tf.r.data[idx] = q[idx];

        wt_tf->weight = det->decision_margin;
    }

    enum ach_status r = sns_msg_wt_tf_put(&marker_chan, msg);
    if (ACH_OK != r)
        ch_error("Failed to put marker message in ach channel.");

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
