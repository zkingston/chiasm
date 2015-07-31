#include <iostream>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include <chiasm.h>

int
ch_load_calibration(struct ch_device *device, const char *filename)
{
    struct ch_calibration *calib =
        (struct ch_calibration *) ch_calloc(1, sizeof(struct ch_calibration));

    if (calib == NULL)
        return (-1);

    cv::FileStorage in((string) filename, cv::FileStorage::READ);

    if (!in.isOpened()) {
        ch_error((const char *) "Failed to open calibration file.");
        free(calib);
        return (-1);
    }

    cv::Size image_size;
    in["image_size"] >> image_size;

    calib->framesize.width = image_size.width;
    calib->framesize.height = image_size.height;

    if (calib->framesize.width != device->framesize.width
        || calib->framesize.height != device->framesize.height) {
        ch_error("Calibration file has mismatched framesize with device.");
        free(calib);
        return (-1);
    }

    cv::Size board_size;
    in["board_size"] >> board_size;
    calib->boardsize.width = board_size.width;
    calib->boardsize.height = board_size.height;

    in["square_size"] >> calib->squaresize;
    in["reprojection_error"] >> calib->reproj_err;

    cv::Mat camera_mat;
    in["camera_matrix"] >> camera_mat;
    {
        size_t idx;
        for (idx = 0; idx < 3; idx++) {
            size_t jdx;
            for (jdx = 0; jdx < 3; jdx++)
                calib->camera_mat[idx][jdx] = camera_mat.at<double>(idx, jdx);
        }
    }


    cv::Mat distort_coeffs;
    in["distortion_coefficients"] >> distort_coeffs;
    {
        size_t idx;
        for (idx = 0; idx < 5; idx++)
            calib->distort_coeffs[idx] = distort_coeffs.at<double>(idx);
    }

    in.release();

    device->calib = calib;

    return (0);
}

void
ch_close_calibration(struct ch_device *device)
{
    if (device->calib)
        free(device->calib);

    device->calib = NULL;
}


void
ch_save_calibration(string filename, cv::Size image_size, cv::Size board_size,
                    double square_size, double reproj_err,
                    cv::Mat camera_mat, cv::Mat distortion_coeffs)
{
    cv::FileStorage out(filename, cv::FileStorage::WRITE);

    out << "image_size" << image_size;
    out << "board_size" << board_size;
    out << "square_size" << square_size;
    out << "reprojection_error" << reproj_err;
    out << "camera_matrix" << camera_mat;
    out << "distortion_coefficients" << distortion_coeffs;

    out.release();
}

/**
 * @brief Convert a ch_frmbuf array image into a OpenCV Mat image.
 *
 * @param buf ch_frmbuf to convert.
 * @param mat Output Mat to fill. Should already be allocated.
 * @param size Image size.
 * @param stride Stride of buf.
 * @param b_per_pix Bytes per pixel in ch_frmbuf.
 * @return None.
 */
void
ch_frmbuf_to_mat(struct ch_frmbuf *buf, cv::Mat &mat, struct ch_rect *size,
                 uint32_t stride, uint32_t b_per_pix)
{
    // If image is a flat array, just copy directly.
    if (stride == size->width * b_per_pix) {
        uint8_t *dest = mat.ptr(0);
        memcpy(dest, buf->start,
               b_per_pix * size->width * size->height);

    // Otherwise, have to deal with the stride seperating image planes.
    } else {
        size_t idx;
        for (idx = 0; idx < size->height; idx++) {
            uint8_t *dest = mat.ptr(idx);
            uint8_t *src = &buf->start[idx * stride];

            memcpy(dest, src, b_per_pix * size->width);
        }
    }
}

/**
 * @brief Convert an OpenCV Mat image into a ch_frmbuf array.
 *
 * @param mat Mat to convert.
 * @param buf ch_frmbuf to output to.
 * @param size Image size.
 * @param stride Stride of buf.
 * @param b_per_pix Bytes per pixel in ch_frmbuf.
 * @return None.
 */
void
ch_mat_to_frmbuf(cv::Mat &mat, struct ch_frmbuf *buf, struct ch_rect *size,
                 uint32_t stride, uint32_t b_per_pix)
{
    // If image is a flat array, just copy directly.
    if (stride == size->width * b_per_pix) {
        uint8_t *src = mat.ptr(0);
        memcpy(buf->start, src,
               b_per_pix * size->width * size->height);

    // Otherwise, have to deal with the stride seperating image planes.
    } else {
        size_t idx;
        for (idx = 0; idx < size->height; idx++) {
            uint8_t *dest = &buf->start[idx * stride];
            uint8_t *src = mat.ptr(idx);

            memcpy(dest, src, b_per_pix * size->width);
        }
    }
}

void
ch_undistort(struct ch_device *device, struct ch_dl_cx *cx, struct ch_frmbuf *buf)
{
    cv::Size image_size(device->framesize.width, device->framesize.height);
    cv::Mat camera_mat(cv::Size(3, 3), CV_64F, device->calib->camera_mat);
    cv::Mat distort_coeffs(cv::Size(5, 1), CV_64F, device->calib->distort_coeffs);

    cv::Mat image(image_size, CV_8UC(cx->b_per_pix));
    ch_frmbuf_to_mat(buf, image, &device->framesize,
                     cx->out_stride, cx->b_per_pix);

    cv::Mat undistorted(image_size, CV_8UC(cx->b_per_pix));
    cv::undistort(image, undistorted, camera_mat, distort_coeffs);

    ch_mat_to_frmbuf(undistorted, buf, &device->framesize,
                     cx->out_stride, cx->b_per_pix);
}
