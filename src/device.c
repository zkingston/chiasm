#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include <chiasm.h>

int ch_stop_stream(struct ch_device *device);

/**
 * @brief Parse a double from a string.
 *
 * @param optarg String to parse.
 * @param value Pointer to variable to fill.
 * @return 0 on success, -1 on failure.
 */
static int
ch_parse_double(char *optarg, double *value)
{
    char *ptr;
    double r = strtod(optarg, &ptr);

    if ((int) r == 0 && ptr == optarg)
        return (-1);

    if (errno == ERANGE)
        return (-1);

    *value = r;
    return (0);
}

/**
 * @brief Parse a uint32_t from a string.
 *
 * @param optarg String to parse.
 * @param value Pointer to variable to fill.
 * @return 0 on success, -1 on failure.
 */
static int
ch_parse_uint32(char *optarg, uint32_t *value)
{
    char *ptr;
    uint32_t r = (uint32_t) strtoul(optarg, &ptr, 10);

    if (r == 0 && ptr == optarg)
        return (-1);

    if (errno == ERANGE)
        return (-1);

    *value = r;
    return (0);
}

int
ch_parse_device_opt(int opt, char *optarg, struct ch_device *device)
{
    switch (opt) {
    case 'd':
        device->name = optarg;
        break;

    case 't': {
        double r;
        if (ch_parse_double(optarg, &r) == -1) {
            fprintf(stderr, "Invalid timeout.\n");
            return (-1);
        }

        device->timeout = ch_sec_to_timeval(r);
        break;
    }

    case 'b': {
        uint32_t r;
        if (ch_parse_uint32(optarg, &r) == -1) {
            fprintf(stderr, "Invalid number of buffers.\n");
            return (-1);
        }

        device->num_buffers = r;
        break;
    }

    case 'f':
        if (strnlen(optarg, 5) > 4) {
            fprintf(stderr, "Pixel formats must be at most 4 characters.\n");
            return (-1);
        }

        device->in_pixfmt = ch_string_to_pixfmt(optarg);
        break;

    case 'g':
        if (sscanf(optarg, "%ux%u",
                   &device->framesize.width, &device->framesize.height) != 2) {
            fprintf(stderr, "Error parsing geometry string.\n");
            return (-1);
        }

        break;

    default:
        fprintf(stderr, "Invalid option for device parse.\n");
        return (-1);
    }

    return (0);
}

/**
 * @brief Robust wrapper around ioctl.
 *
 * @param fd File-descriptor.
 * @param request ioctl request.
 * @param arg Arguments to request.
 * @return 0 on success, -1 on failure. 1 on EINVAL.
 */
int
ch_ioctl(struct ch_device *device, int request, void *arg)
{
    int r;

    // If device is closed, do nothing and error.
    if (device->fd == 0)
        return (-1);

    // Loop request until not interrupted.
    do {
        r = ioctl(device->fd, request, arg);
    } while (r == -1 && errno == EINTR);

    if (r == -1) {
        // No output on EINVAL, used to determine end of enumeration.
        if (errno != EINVAL)
            ch_error_no("ioctl failure.", errno);

        else
            return (1);

        // If device no longer exists, close the device.
        if (errno == ENODEV) {
            ch_close_device(device);

            // Clean up allocated structures.
            ch_stop_stream(device);
        }

        return (-1);
    }

    return (0);
}

/**
 * @brief Checks a video device's capabilities for needed support.
 *
 * @param device Device to validate.
 * @return 0 on success, -1 on failure.
 */
static int
ch_validate_device(struct ch_device *device)
{
    struct v4l2_capability caps;
    CH_CLEAR(&caps);

    // Query device for capabilities.
    if (ch_ioctl(device, VIDIOC_QUERYCAP, &caps) == -1)
        return (-1);

    // Verify video capture is supported.
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ch_error("Device does not support video capture.");
        return (-1);
    }

    // Verify streaming is supported.
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        ch_error("Device does not support streaming I/O.");
        return (-1);
    }

    return (0);
}

void
ch_init_device(struct ch_device *device)
{
    device->name = (char *) CH_DEFAULT_DEVICE;
    device->fd = 0;
    pthread_mutex_init(&device->mutex, NULL);

    device->in_buffers = NULL;
    device->num_buffers = CH_DEFAULT_BUFNUM;

    device->framesize = (struct ch_rect) {CH_DEFAULT_WIDTH, CH_DEFAULT_HEIGHT};
    device->in_pixfmt = ch_string_to_pixfmt(CH_DEFAULT_FORMAT);

    device->timeout = ch_sec_to_timeval(CH_DEFAULT_TIMEOUT);
    device->stream = false;
    device->fps = 0.0;

    device->calib = NULL;
}

int
ch_open_device(struct ch_device *device)
{
    struct stat st;

    // Verify existence of device.
    if (stat(device->name, &st) == -1) {
        ch_error_no("Failed to find device.", errno);
        return (-1);
    }

    // Verify device is a character device.
    if (!S_ISCHR(st.st_mode)) {
        ch_error("Device is not a character device.");
        return (-1);
    }

    // Open device in read/write non-blocking mode.
    if ((device->fd = open(device->name, O_RDWR | O_NONBLOCK)) == -1) {
        ch_error_no("Failed to open device.", errno);
        goto error;
    }

    // Verify device can stream video.
    if (ch_validate_device(device) == -1) {
        ch_close_device(device);
        goto error;
    }

    return (0);

error:
    device->fd = 0;
    return (-1);
}

int
ch_close_device(struct ch_device *device)
{
    // Only close file-descriptor if still open.
    if (device->fd > 0) {
        if (close(device->fd) == -1) {
            ch_error_no("Failed to close device.", errno);
            return (-1);
        }

        device->fd = 0;
    }

    return (0);
}

struct ch_fmts *
ch_enum_fmts(struct ch_device *device)
{
    // Find maximum format index.
    struct v4l2_fmtdesc fmtdesc;
    CH_CLEAR(&fmtdesc);

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int r = 0;
    while ((r = ch_ioctl(device, VIDIOC_ENUM_FMT, &fmtdesc)) != 1)
        fmtdesc.index++;

    if (r == -1)
        return (NULL);

    // Create storage array and fill.
    struct ch_fmts *fmts =
        (struct ch_fmts *) ch_calloc(1, sizeof(struct ch_fmts));

    if (fmts == NULL)
        return (NULL);

    fmts->length = fmtdesc.index;

    fmts->fmts = (uint32_t *) ch_calloc(fmtdesc.index, sizeof(uint32_t));
    if (fmts->fmts == NULL) {
        free(fmts);
        return (NULL);
    }

    // Reiterate and fill array.
    fmtdesc.index = 0;
    while ((r = ch_ioctl(device, VIDIOC_ENUM_FMT, &fmtdesc)) != 1)
        fmts->fmts[fmtdesc.index++] = fmtdesc.pixelformat;

    if (r == -1) {
        ch_destroy_fmts(fmts);
        fmts = NULL;
    }

    return (fmts);
}

void
ch_destroy_fmts(struct ch_fmts *fmts)
{
    free(fmts->fmts);
    free(fmts);
}

struct ch_frmsizes *
ch_enum_frmsizes(struct ch_device *device)
{
    // Find maximum size index.
    struct v4l2_frmsizeenum frmsize;
    CH_CLEAR(&frmsize);

    frmsize.index = 0;
    frmsize.pixel_format = device->in_pixfmt;

    int r = 0;
    while ((r = ch_ioctl(device, VIDIOC_ENUM_FRAMESIZES, &frmsize)) != 1)
        frmsize.index++;

    if (r == -1)
        return (NULL);

    // Only supporting discrete resolutions. Should return on first if not.
    if (frmsize.type != V4L2_FRMSIZE_TYPE_DISCRETE)
        return (NULL);

    // Create storage array and fill.
    struct ch_frmsizes *frmsizes =
        (struct ch_frmsizes *) ch_calloc(1, sizeof(struct ch_frmsizes));

    if (frmsizes == NULL)
        return (NULL);

    frmsizes->length = frmsize.index;

    frmsizes->frmsizes =
        (struct ch_rect *) ch_calloc(frmsize.index, sizeof(struct ch_rect));

    if (frmsizes->frmsizes == NULL) {
        free(frmsizes);
        return (NULL);
    }

    // Reiterate and fill array.
    frmsize.index = 0;
    while ((r = ch_ioctl(device, VIDIOC_ENUM_FRAMESIZES, &frmsize)) != 1)
        frmsizes->frmsizes[frmsize.index++] = (struct ch_rect) {
            frmsize.discrete.width,
                frmsize.discrete.height
        };

    if (r == -1) {
        ch_destroy_frmsizes(frmsizes);
        frmsizes = NULL;
    }

    return (frmsizes);
}

void
ch_destroy_frmsizes(struct ch_frmsizes *frmsizes)
{
    free(frmsizes->frmsizes);
    free(frmsizes);
}

double
ch_get_fps(struct ch_device *device)
{
    struct v4l2_frmivalenum frmival;
    CH_CLEAR(&frmival);

    frmival.index = 0;
    frmival.pixel_format = device->in_pixfmt;
    frmival.width = device->framesize.width;
    frmival.height = device->framesize.height;

    double frmrate = 0.0;

    while (ch_ioctl(device, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0) {
        // Only supporting discrete frame intervals.
        if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            double temp_frmrate = 1.0 /
                ((double) frmival.discrete.numerator
                 / (double) frmival.discrete.denominator);

            if (temp_frmrate > frmrate)
                frmrate = temp_frmrate;
        }

        frmival.index++;
    }

    return (frmrate);
}

/**
 * @brief Helper function to iterate over a range of camera controls.
 *
 * @param device Device to query.
 * @param base Base index of control range.
 * @param limit Exclusive limit of control range.
 * @param callback Function to call on each valid control.
 *        Should return 0 on succes, -1 on failure.
 * @param cx Context to provide to callback function.
 * @return 0 on success, -1 on failure.
 */
static int
ch_enum_ctrl_range(struct ch_device *device, uint32_t base, uint32_t limit,
		   int (*callback)(void *cx, struct v4l2_queryctrl *qctrl),
		   void *cx)
{
    struct v4l2_queryctrl qctrl;

    uint32_t idx;
    for (idx = base; idx < limit; idx++) {
	CH_CLEAR(&qctrl);
	qctrl.id = idx;

	if (ch_ioctl(device, VIDIOC_QUERYCTRL, &qctrl) == -1)
	    return (-1);

	// Using population of the name field to determine valid controls.
	if (qctrl.name[0] != '\0')
	    if (callback(cx, &qctrl) == -1)
		return (-1);
    }

    return (0);
}

/**
 * @brief Helper function for finding total number of controls.
 */
static int
ch_length_inc(void *cx, struct v4l2_queryctrl *qctrl) {
    // No warnings.
    qctrl = (struct v4l2_queryctrl *) qctrl;

    uint32_t *length = (uint32_t *) cx;
    (*length)++;
    return (0);
}

/**
 * @brief Helper function for populating struct ch_ctrls.
 */
static int
ch_ctrls_populate(void *cx, struct v4l2_queryctrl *qctrl) {
    struct ch_ctrls *ctrls = (struct ch_ctrls *) cx;
    struct ch_ctrl *ctrl = &ctrls->ctrls[ctrls->length];

    ctrl->id = qctrl->id;
    memcpy(ctrl->name, qctrl->name, 32);
    ctrl->type = qctrl->type;
    ctrl->min = qctrl->minimum;
    ctrl->max = qctrl->maximum;
    ctrl->step = qctrl->step;
    ctrl->defval = qctrl->default_value;

    ctrls->length++;
    return (0);
}

struct ch_ctrls *
ch_enum_ctrls(struct ch_device *device)
{
    // TODO: Iterate over private controls. Not needed right now.
    uint32_t length = 0;

    // Iterate over predefined controls.
    if (ch_enum_ctrl_range(device, V4L2_CID_BASE, V4L2_CID_LASTP1,
			   ch_length_inc, &length) == -1)
	return (NULL);

    // Iterate over camera controls.
    if (ch_enum_ctrl_range(device,
			   V4L2_CID_CAMERA_CLASS_BASE,
			   V4L2_CID_AUTO_FOCUS_RANGE + 1,
			   ch_length_inc, &length) == -1)
	return (NULL);

    // Create storage array and fill.
    struct ch_ctrls *ctrls =
        (struct ch_ctrls *) ch_calloc(1, sizeof(struct ch_ctrls));

    if (ctrls == NULL)
        return (NULL);

    ctrls->ctrls = (struct ch_ctrl *) ch_calloc(length, sizeof(struct ch_ctrl));
    if (ctrls->ctrls == NULL) {
        free(ctrls);
        return (NULL);
    }

    if (ch_enum_ctrl_range(device, V4L2_CID_BASE, V4L2_CID_LASTP1,
			   ch_ctrls_populate, ctrls) == -1)
	goto clean;

    if (ch_enum_ctrl_range(device,
			   V4L2_CID_CAMERA_CLASS_BASE,
			   V4L2_CID_AUTO_FOCUS_RANGE + 1,
			   ch_ctrls_populate, ctrls) == -1)
	goto clean;

    return (ctrls);

clean:
    // Set shorter length to prevent iteration over uninitialized elements.
    ch_destroy_ctrls(ctrls);

    return (NULL);
}

void
ch_destroy_ctrls(struct ch_ctrls *ctrls)
{
    free(ctrls->ctrls);
    free(ctrls);
}

struct ch_ctrl_menu *
ch_enum_ctrl_menu(struct ch_device *device, struct ch_ctrl *ctrl)
{
    struct v4l2_querymenu qmenu;
    CH_CLEAR(&qmenu);

    struct ch_ctrl_menu *menu =
        (struct ch_ctrl_menu *) ch_calloc(1, sizeof(struct ch_ctrl_menu));

    if (menu == NULL)
	return (NULL);

    menu->length = ctrl->max - ctrl->min + 1;
    menu->items = (union ch_ctrl_menu_item *)
        ch_calloc(menu->length, sizeof(union ch_ctrl_menu_item));

    if (menu->items == NULL) {
	free(menu);
	return (NULL);
    }

    // Assume that the control's maximum will be positive.
    qmenu.id = ctrl->id;
    for (qmenu.index = ctrl->min; qmenu.index <= (uint32_t) ctrl->max; qmenu.index++) {
	if (ch_ioctl(device, VIDIOC_QUERYMENU, &qmenu) == -1)
	    goto clean;

	memcpy(menu->items[qmenu.index - ctrl->min].name, qmenu.name, 32);
    }

    return (menu);

clean:
    ch_destroy_ctrl_menu(menu);
    return (NULL);
}

void
ch_destroy_ctrl_menu(struct ch_ctrl_menu *menu)
{
    free(menu->items);
    free(menu);
}

struct ch_ctrl *
ch_find_ctrl(struct ch_device *device, const char *ctrl_name)
{
    struct ch_ctrls *ctrls = ch_enum_ctrls(device);
    if (ctrls == NULL)
	return (NULL);

    size_t l = strnlen(ctrl_name, 32);

    // Iterate over controls and find the matching name.
    size_t idx;
    struct ch_ctrl *ctrl = NULL;
    for (idx = 0; idx < ctrls->length; idx++)
	if (strncmp(ctrl_name, ctrls->ctrls[idx].name, l) == 0
	    && l == strnlen(ctrls->ctrls[idx].name, 32))
	    ctrl = &ctrls->ctrls[idx];

    if (ctrl == NULL)
	ch_error("Control not found.");

    else {
	struct ch_ctrl *ctrl_t =
            (struct ch_ctrl *) ch_calloc(1, sizeof(struct ch_ctrl));

	if (ctrl_t == NULL) {
	    ctrl = NULL;
	    goto clean;
	}

	memcpy(ctrl_t, ctrl, sizeof(struct ch_ctrl));
	ctrl = ctrl_t;
    }

clean:
    ch_destroy_ctrls(ctrls);
    return (ctrl);
}

int
ch_get_ctrl(struct ch_device *device, struct ch_ctrl *ctrl, int32_t *value)
{
    struct v4l2_control vctrl;
    CH_CLEAR(&vctrl);

    vctrl.id = ctrl->id;
    if (ch_ioctl(device, VIDIOC_G_CTRL, &vctrl) == -1)
	return (-1);

    *value = vctrl.value;
    return (0);
}

int
ch_set_ctrl(struct ch_device *device, struct ch_ctrl *ctrl, int32_t value)
{
    // Invalid value.
    if (value > ctrl->max / ctrl->step || value < ctrl->min / ctrl->step)
	return (-1);

    struct v4l2_control vctrl;
    CH_CLEAR(&vctrl);

    vctrl.id = ctrl->id;
    vctrl.value = value * ctrl->step;

    if (ch_ioctl(device, VIDIOC_S_CTRL, &vctrl) == -1)
	return (-1);

    return (0);
}

/**
 * @brief Validates a device's requested format.
 *
 * @param device The device to validate.
 * @return 0 on success, -1 on failure.
 */
static int
ch_validate_fmt(struct ch_device *device)
{
    struct ch_fmts *fmts = ch_enum_fmts(device);
    if (fmts == NULL)
        return (-1);

    size_t idx;
    for (idx = 0; idx < fmts->length; idx++)
        if (fmts->fmts[idx] == device->in_pixfmt)
            break;

    int r = 0;
    if (idx == fmts->length) {
        ch_error("Format is unsupported by device.");

        r = -1;
    }

    ch_destroy_fmts(fmts);

    return (r);
}

/**
 * @brief Validates a device's requested framesize.
 *
 * @param device The device to validate.
 * @return 0 on success, -1 on failure.
 */
static int
ch_validate_frmsize(struct ch_device *device)
{
    struct ch_frmsizes *frmsizes = ch_enum_frmsizes(device);

    size_t idx;
    for (idx = 0; idx < frmsizes->length; idx++)
        if (frmsizes->frmsizes[idx].width == device->framesize.width
                && frmsizes->frmsizes[idx].height == device->framesize.height)
            break;

    int r = 0;
    if (idx == frmsizes->length) {
        ch_error("Framesize is unsupported for format.\n");
        r = -1;
    }

    ch_destroy_frmsizes(frmsizes);

    return (r);
}

int
ch_set_fmt(struct ch_device *device)
{
    // Validate request.
    if (ch_validate_fmt(device) == -1)
        return (-1);

    if (ch_validate_frmsize(device) == -1)
        return (-1);

    // Set format and framesize on device.
    struct v4l2_format fmt;
    CH_CLEAR(&fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = device->framesize.width;
    fmt.fmt.pix.height = device->framesize.height;
    fmt.fmt.pix.pixelformat = device->in_pixfmt;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.bytesperline = 0;

    if (ch_ioctl(device, VIDIOC_S_FMT, &fmt) == -1) {
        ch_error("Failed to set output format.");
        return (-1);
    }

    return (0);
}

/**
 * @brief Unmaps memory-mapped streaming buffers.
 *
 * @param device Device to unmap from.
 * @return 0 on success, -1 on failure.
 */
static int
ch_unmap_buffers(struct ch_device *device)
{
    size_t idx;
    for (idx = 0; idx < device->num_buffers; idx++) {
        // Only unmap mapped buffers.
        if (device->in_buffers[idx].start == NULL)
            continue;

        if (munmap(device->in_buffers[idx].start,
                    device->in_buffers[idx].length) == -1) {
            ch_error_no("Failed to munmap buffer.", errno);

            return (-1);
        }
    }

    return (0);
}

/**
 * @brief Maps memory-mapped streaming buffers for a device.
 *
 * @param device Device to map buffers for.
 * @return 0 on succes, -1 on failure.
 */
static int
ch_map_buffers(struct ch_device *device)
{
    struct v4l2_requestbuffers req;
    CH_CLEAR(&req);

    req.count = device->num_buffers;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    // Request a number of buffers.
    if (ch_ioctl(device, VIDIOC_REQBUFS, &req) == -1) {
        ch_error("Failed to request buffers.");
        return (-1);
    }

    // Compare return to requested amount of buffers.
    if (req.count != device->num_buffers) {
        ch_error("Insufficient memory on device for number of buffers.\n");
        return (-1);
    }

    // Allocate buffers.
    device->in_buffers =
        (struct ch_frmbuf *) ch_calloc(req.count, sizeof(struct ch_frmbuf));

    if (device->in_buffers == NULL)
        return (-1);

    // Query each buffer and map it into our address space.
    size_t idx;
    for (idx = 0; idx < req.count; idx++) {
        struct v4l2_buffer buf;
        CH_CLEAR(&buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = idx;

        if (ch_ioctl(device, VIDIOC_QUERYBUF, &buf) == -1) {
            ch_error("Failed to query buffers.");
            goto error;
        }

        device->in_buffers[idx].length = buf.length;
        device->in_buffers[idx].start = (uint8_t *) mmap(
                NULL,
                buf.length,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                device->fd,
                buf.m.offset
        );

        if (device->in_buffers[idx].start == MAP_FAILED) {
            ch_error_no("Failed to map buffers.", errno);
            goto error;
        }
    }

    return (0);

error:
    ch_unmap_buffers(device);
    free(device->in_buffers);

    return (-1);
}

/**
 * @brief Begin streaming from device.
 *
 * @param device Device to begin streaming from.
 * @return 0 on success, -1 on failure.
 */
int
ch_start_stream(struct ch_device *device)
{
    if (ch_map_buffers(device) == -1)
        return (-1);

    enum v4l2_buf_type type;

    // Query each buffer to be filled.
    size_t idx;
    for (idx = 0; idx < device->num_buffers; idx++) {
        struct v4l2_buffer buf;
        CH_CLEAR(&buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = idx;

        if (ch_ioctl(device, VIDIOC_QBUF, &buf) == -1) {
            ch_error("Failed to request buffer.");
            goto error;
        }
    }

    // Start streaming.
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ch_ioctl(device, VIDIOC_STREAMON, &type) == -1) {
        ch_error("Failed to start stream.");
        goto error;
    }

    device->stream = true;
    return (0);

error:
    ch_unmap_buffers(device);
    return (-1);
}

/**
 * @brief Stop streaming from a device.
 *
 * @param device Device to stop streaming from.
 * @return 0 on succes, -1 on failure.
 */
int
ch_stop_stream(struct ch_device *device)
{
    device->stream = false;

    // Send command to device to stop stream.
    if (device->fd > 0) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (ch_ioctl(device, VIDIOC_STREAMOFF, &type) == -1) {
            ch_error("Failed to stop stream.");
            return (-1);
        }
    }

    // Obtain lock on stream buffers.
    pthread_mutex_lock(&device->mutex);

    // Destroy input buffers.
    if (device->in_buffers)
        ch_unmap_buffers(device);

    free(device->in_buffers);
    device->in_buffers = NULL;

    pthread_mutex_unlock(&device->mutex);

    return (0);
}

int
ch_stream(struct ch_device *device, struct ch_dl **plugins, uint32_t n_plugins)
{
    if (device->stream) {
        ch_error("Device is already streaming.");
        return (-1);
    }

    struct timespec ts;
    double pt = -1;

    int r = 0;
    // Initialize and create plugin context and threads.
    if ((r = ch_init_plugins(device, plugins, n_plugins)) == -1)
        goto clean;

    // Start streaming from the camera.
    if ((r = ch_start_stream(device)) == -1)
        goto clean;

    struct ch_decode_cx decode;
    // Initialize decoding context.
    if ((r = ch_init_decode_cx(device, &decode)) == -1)
        goto clean;

    while (device->stream) {
        // Update FPS
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double t = ch_timespec_to_sec(ts);

        if (pt > 0)
            device->fps = (1.0 - CH_FPS_UPDATE) * device->fps
                + CH_FPS_UPDATE * (1.0 / (t - pt));
        pt = t;

        // Wait on select for a new frame.
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(device->fd, &fds);

        struct timeval temp = device->timeout;
        r = select(device->fd + 1, &fds, NULL, NULL, &temp);

        if (r == -1) {
	    if (errno == EINTR) {
		r = 0;
		continue;
	    }

            ch_error_no("Error on select.", errno);
            break;

        } else if (r == 0) {
            ch_error("Timeout on select.");
            r = -1;
            break;
        }

        // Verify we are still streaming after select.
        if (!device->stream)
            break;

        // Dequeue buffer.
        struct v4l2_buffer buf;
        CH_CLEAR(&buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if ((r = ch_ioctl(device, VIDIOC_DQBUF, &buf)) == -1) {
            ch_error("Failure dequeing buffer.");
            break;
        }

        // Verify buffer is valid.
        if (buf.index > device->num_buffers) {
            ch_error("Bad buffer index returned from dequeue.");
            r = -1;
            break;
        }

        // Set current size of input buffer.
	device->in_buffers[buf.index].length = buf.bytesused;

        // Decode the new frame.
        if ((r = ch_decode(device, &device->in_buffers[buf.index], &decode)) == -1)
            break;

        if ((r = ch_update_plugins(device, &decode, plugins, n_plugins)) == -1)
            break;

        // Queue buffer.
        if ((r = ch_ioctl(device, VIDIOC_QBUF, &buf)) == -1) {
            ch_error("Failure requeing buffer.");
            break;
        }
    }

clean:
    ch_destroy_decode_cx(&decode);
    ch_quit_plugins(plugins, n_plugins);
    ch_stop_stream(device);

    return (r);
}
