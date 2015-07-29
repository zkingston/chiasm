#include <stdlib.h>
#include <stdio.h>

#include <libavcodec/avcodec.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include <chiasm.h>

using namespace std;
using namespace cv;

Size boardsize(8, 6);      // Geometry of the board.
double square_size = 10.0; // Size of each square on the board in millimeters.
int calib_flag = 0;        // Calibration flags.
string out_filename = "calibration.xml";

vector< vector<Point2f> > image_points;

int
CH_DL_INIT(struct ch_device *device, struct ch_dl_cx *cx)
{
    calib_flag |= CV_CALIB_FIX_PRINCIPAL_POINT;
    calib_flag |= CV_CALIB_FIX_ASPECT_RATIO;

    return (0);
}

int
CH_DL_CALL(struct ch_frmbuf *in_buf)
{

    return (0);
}

int
CH_DL_QUIT(void)
{

    return (0);
}
