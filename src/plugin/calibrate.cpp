#include <stdlib.h>
#include <stdio.h>

#include <iostream>

#include <libavcodec/avcodec.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <chiasm.h>

using namespace std;

int    calib_flag = 0; // Calibration flags.
int    chess_flag = 0; // Calibration board finding flags.
string out_filename = "calibration.xml";

cv::Size image_size;          // Size of the image.
cv::Size board_size(4, 4);    // Size of the calibration board.
double   square_size = 29.0;  // Size of a square on the calibration board in mm.

cv::Size search_size(11, 11); // Subpixel refinement search window.
int      search_iter = 30;    // Subpixel refinement maximum iteration count.
double   search_eps  = 0.05;  // Subpixel refinement delta episilon criteria.

// Subpixel refinement criteria.
cv::TermCriteria criteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER,
                          search_iter, search_eps);

vector< vector< cv::Point2f > > image_points; // Found calibration board points.

double wait_time = 2.0;     // Time in seconds to wait inbetween shots.
double previous_time = 0.0; // Previous timestamp for found board.

int
CH_DL_INIT(struct ch_device *device, struct ch_dl_cx *cx)
{
    cx->out_pixfmt = AV_PIX_FMT_GRAY8;

    // Set flags for calibration.
    calib_flag |= CV_CALIB_FIX_PRINCIPAL_POINT;
    calib_flag |= CV_CALIB_FIX_ASPECT_RATIO;
    calib_flag |= CV_CALIB_FIX_K4;
    calib_flag |= CV_CALIB_FIX_K5;

    // Set flags for finding calibration board.
    chess_flag |= CV_CALIB_CB_ADAPTIVE_THRESH;
    chess_flag |= CV_CALIB_CB_FAST_CHECK;
    chess_flag |= CV_CALIB_CB_NORMALIZE_IMAGE;

    image_size = cv::Size(device->framesize.width,
                          device->framesize.height);

    return (0);
}

int
CH_DL_CALL(struct ch_frmbuf *in_buf)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double current_time = ch_timespec_to_sec(ts);

    // Check if enough time has passed.
    if (current_time - previous_time <= wait_time)
        return (0);

    cv::Mat image(image_size, CV_8UC1, in_buf->start);
    vector< cv::Point2f > image_point;

    // Find chessboard corners.
    bool found =
        cv::findChessboardCorners(image, board_size, image_point, chess_flag);

    if (!found)
        return (0);

    cerr << "Found calibration board!" << endl;
    cerr.flush();

    // Set time.
    previous_time = current_time;

    // Refine pixel locations of corners.
    cv::cornerSubPix(image, image_point,
                    search_size, cv::Size(-1, -1), criteria);

    // Add corner points to current set.
    image_points.push_back(image_point);
    return (0);
}

int
CH_DL_QUIT(void)
{
    // If no chessboards have been found, don't bother calibrating.
    if (image_points.size() == 0)
        return (0);

    // Create reference object points.
    vector< cv::Point3f > corners;
    {
        int idx;
        for (idx = 0; idx < board_size.width; idx++) {
            int jdx;
            for (jdx = 0; jdx < board_size.height; jdx++)
                corners.push_back(
                    cv::Point3f(square_size * idx, square_size * jdx, 0));
        }
    }

    vector< vector < cv::Point3f > > object_points;
    object_points.resize(image_points.size(), corners);

    // Create camera project matrix.
    cv::Mat camera_mat = cv::Mat::eye(3, 3, CV_64F);

    // If fixed aspect ratio, set F_x to 0.
    if (calib_flag & CV_CALIB_FIX_ASPECT_RATIO)
        camera_mat.at< double >(0, 0) = 1.0;

    cv::Mat distortion_coeffs = cv::Mat::zeros(8, 1, CV_64F);
    vector< cv::Mat > rotation_vec, translation_vec;

    double error = cv::calibrateCamera(
        object_points, image_points, image_size, camera_mat,
        distortion_coeffs, rotation_vec, translation_vec, calib_flag);

    cerr << "Camera calibrated. Reprojection error: " << error << endl;
    cerr << "Intrisic Matrix:" << endl;
    cerr << camera_mat << endl;
    cerr << "Distortion Coefficients:" << endl;
    cerr << distortion_coeffs << endl;

    ch_save_calibration(out_filename, image_size, board_size, square_size,
                        error, camera_mat, distortion_coeffs);

    return (0);
}
