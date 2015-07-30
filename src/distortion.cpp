#include <iostream>

#include <opencv2/core/core.hpp>

#include <chiasm.h>


// Load calibration file
struct ch_calibration *
ch_load_calibration(const char *filename)
{
    struct ch_calibration *calib =
        (struct ch_calibration *) ch_calloc(1, sizeof(struct ch_calibration));

    if (calib == NULL)
        return (NULL);

    cv::FileStorage in((string) filename, cv::FileStorage::READ);

    if (!in.isOpened()) {
        ch_error((const char *) "Failed to open calibration file.");
        return (NULL);
    }

    cv::Size image_size;
    in["image_size"] >> image_size;
    calib->framesize.width = image_size.width;
    calib->framesize.height = image_size.height;

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

    return (calib);
}

// Save calibration file
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
