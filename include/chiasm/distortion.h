#ifndef CHIASM_DISTORTION_H_
#define CHIASM_DISTORTION_H_

#ifdef __cplusplus

#include <opencv2/core/core.hpp>

using namespace std;

void ch_save_calibration(string filename, cv::Size image_size,
                         cv::Size board_size, double square_size,
                         double reproj_err, cv::Mat camera_mat,
                         cv::Mat distortion_coeffs);

#endif

#endif
