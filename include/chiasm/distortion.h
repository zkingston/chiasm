#ifndef CHIASM_DISTORTION_H_
#define CHIASM_DISTORTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <chiasm/types.h>

struct ch_calibration *ch_load_calibration(const char *filename);

void ch_undistort(struct ch_device *device, struct ch_dl_cx *cx, struct ch_frmbuf *buf);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <opencv2/core/core.hpp>

using namespace std;

void ch_save_calibration(string filename, cv::Size image_size,
                         cv::Size board_size, double square_size,
                         double reproj_err, cv::Mat camera_mat,
                         cv::Mat distortion_coeffs);

#endif

#endif
