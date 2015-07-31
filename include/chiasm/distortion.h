#ifndef CHIASM_DISTORTION_H_
#define CHIASM_DISTORTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <chiasm/types.h>

/**
 * @brief Load a camera calibration saved from a file.
 *
 * @param filename Filename of calibration file.
 * @return Allocated calibration file on success, NULL on failure.
 */
struct ch_calibration *ch_load_calibration(const char *filename);

/**
 * @brief Undistorts an image based on a calibrated camera's parameters.
 *
 * @param device Device image was taken on.
 * @param cx Context for plugin the image is being undistorted for.
 * @param buf The image to undistort.
 * @return None. Changes contents of buf.
 */
void ch_undistort(struct ch_device *device, struct ch_dl_cx *cx, struct ch_frmbuf *buf);

#ifdef __cplusplus
}
#endif

// Only show C++ functions if C++
#ifdef __cplusplus

#include <opencv2/core/core.hpp>

using namespace std;

/**
 * @brief Save a calibrated camera's parameters.
 *
 * @param filename Filename to save calibration to.
 * @param image_size Size of image calibrated on.
 * @param board_size Size of calibration board.
 * @param square_size Size of squares on calibration board.
 * @param reproj_err Reprojection error of calibration.
 * @param camera_mat Intrisic camera parameter matrix.
 * @param distortion_coeffs Distortion coefficients for camera.
 * @return None.
 */
void ch_save_calibration(string filename, cv::Size image_size,
                         cv::Size board_size, double square_size,
                         double reproj_err, cv::Mat camera_mat,
                         cv::Mat distortion_coeffs);

#endif

#endif
