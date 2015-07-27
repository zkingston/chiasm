#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// Apriltag includes.
#include <apriltag.h>
#include <tag36h11.h>
#include <common/zarray.h>
#include <common/image_u8.h>

#include <chiasm.h>

apriltag_family_t *tag_family = NULL;
apriltag_detector_t *tag_detector = NULL;
image_u8_t *saved_image = NULL;

pthread_t thread = 0;
pthread_mutex_t worker_mutex;
pthread_cond_t worker_cond;

bool active = false;

double p[100][4][2];
size_t pc = 0;

static void *
worker_thread(void *arg)
{
    struct ch_device *device = (struct ch_device *) arg;

    while (active) {
        pthread_mutex_lock(&worker_mutex);

        if (saved_image == NULL)
            pthread_cond_wait(&worker_cond, &worker_mutex);

        if (saved_image == NULL)
            break;

        zarray_t *detections = apriltag_detector_detect(tag_detector, saved_image);
        pc = zarray_size(detections);

        int i;
        for (i = 0; i < zarray_size(detections); i++) {
            apriltag_detection_t *det;
            zarray_get(detections, i, &det);

            size_t x;
            for (x = 0; x < 4; x++) {
                p[i][x][0] = det->p[x][0];
                p[i][x][1] = det->p[x][1];
            }
        }

        apriltag_detections_destroy(detections);

        image_u8_destroy(saved_image);
        saved_image = NULL;

        pthread_mutex_unlock(&worker_mutex);
    }

    return (NULL);
}

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

    tag_detector->quad_decimate = 3;
    tag_detector->quad_sigma = 0.0;
    tag_detector->nthreads = 128;
    tag_detector->debug = 0;

    tag_detector->refine_edges = 1;
    tag_detector->refine_decode = 0;
    tag_detector->refine_pose = 0;

    active = true;

    int r;
    if ((r = pthread_create(&thread, NULL, worker_thread, device)) != 0) {
	ch_error_no("Failed to create tagging thread.", r);
	return (-1);
    }

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

    if (saved_image == NULL) {
        saved_image = image_u8_copy(&image);
        pthread_cond_signal(&worker_cond);
    }

    if (pc) {
        size_t i;
        for (i = 0; i < pc; i++) {
            size_t x;
            for (x = 0; x < 4; x++)
                image_u8_draw_line(&image,
                                   p[i][x][0], p[i][x][1],
                                   p[i][(x + 1) % 4][0], p[i][(x + 1) % 4][1],
                                   255, 3);
        }
    }

    return (0);
}

int
CH_DL_QUIT(struct ch_device *device)
{
    device = (struct ch_device *) device;

    pthread_cond_signal(&worker_cond);
    active = false;

    int r;
    if ((r = pthread_join(thread, NULL)) != 0) {
	ch_error_no("Failed to join tagging thread.", r);
	return (-1);
    }

    apriltag_detector_destroy(tag_detector);
    tag36h11_destroy(tag_family);

    tag_detector = NULL;
    tag_family = NULL;

    return (0);
}
