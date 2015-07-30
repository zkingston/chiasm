#include <iostream>

#include <opencv2/core/core.hpp>

#include <chiasm.h>

// Load calibration file

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
