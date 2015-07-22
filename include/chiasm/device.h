#ifndef CHIASM_DEVICE_H_
#define CHIASM_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parses a command line option and switch for a device parameter.
 *        Accepted parameters are detailed in CH_OPTS.
 *
 * @param opt Character representing option.
 * @param optarg Argument.
 * @param device Device to fill in parameter.
 * @return 0 on successful parse, -1 on failure.
 */
int ch_parse_device_opt(int opt, char *optarg, struct ch_device *device);

/**
 * @brief Initialize a struct ch_device.
 *
 * @param device The struct to fill.
 * @return None, fills in provided struct.
 */
void ch_init_device(struct ch_device *device);

/**
 * @brief Opens video device specified by name field in device.
 *
 * @param device The struct ch_device to open the device of.
 * @return 0 on success, -1 on failure. File-descriptor of device will be
 *         filled.
 */
int ch_open_device(struct ch_device *device);

/**
 * @brief Closes an opened video device.
 *
 * @param device Device to close and clean up.
 * @return 0 upon success, -1 upon failure.
 */
int ch_close_device(struct ch_device *device);

/**
 * @brief Return a list of all supported pixel formats for a device.
 *
 * @param device An opened device.
 * @return An allocated struct ch_fmts of all pixel formats available.
 */
struct ch_fmts *ch_enum_fmts(struct ch_device *device);

/**
 * @brief Deallocate a struct ch_fmts created by ch_enum_formats.
 *
 * @param fmts Allocated struct ch_fmts.
 * @return None.
 */
void ch_destroy_fmts(struct ch_fmts *fmts);

/**
 * @brief Return a list of all supported framesizes for a device's selected
 *        format.
 *
 * @param device An opened device.
 * @return An allocated struct ch_frmsizes of all framesizes available.
 */
struct ch_frmsizes *ch_enum_frmsizes(struct ch_device *device);

/**
 * @brief Deallocate a struct ch_frmsizes created by ch_enum_frmsizes.
 *
 * @param frmsizes Allocated struct ch_frmsizes.
 * @return None.
 */
void ch_destroy_frmsizes(struct ch_frmsizes *frmsizes);

/**
 * @brief Return a list of all supported controls on a device.
 *
 * @param device Device to list controls for.
 * @return An allocated struct ch_ctrls of all controls available.
 */
struct ch_ctrls *ch_enum_ctrls(struct ch_device *device);

/**
 * @brief Deallocate a struct ch_ctrls created by ch_enum_ctrls.
 *
 * @param ctrls Allocated struct ch_ctrls.
 * @return None.
 */
void ch_destroy_ctrls(struct ch_ctrls *ctrls);

/**
 * @brief Return a list of all menu options for a control.
 *
 * @param device Device with the control.
 * @param ctrl Menu control to list options for.
 * @return An allocated struct ch_ctrl_menu with all menu options.
 */
struct ch_ctrl_menu *ch_enum_ctrl_menu(struct ch_device *device, struct ch_ctrl *ctrl);

/**
 * @brief Deallocate a struct ch_ctrl_menu created by ch_enum_ctrl_menu.
 *
 * @param menu Allocated struct ch_ctrl_menu.
 * @return None.
 */
void ch_destroy_ctrl_menu(struct ch_ctrl_menu *menu);

/**
 * @brief Find a control on a device.
 *
 * @param device Device with the control.
 * @param ctrl_name Name of control to find.
 * @return An allocated structure of control information. Must be freed.
 */
struct ch_ctrl *ch_find_ctrl(struct ch_device *device, const char *ctrl_name);

/**
 * @brief Get the current value of a control.
 *
 * @param device Device to get control from.
 * @param ctrl The control to get the value for.
 * @return 0 on success, -1 on failure. Sets provided argument.
 */
int ch_get_ctrl(struct ch_device *device, struct ch_ctrl *ctrl, int32_t *value);

/**
 * @brief Set a value of a control on a device.
 *
 * @param device Device to set control on.
 * @param ctrl The control to set the value for.
 * @param value Value to set.
 * @return 0 on success, -1 on failure.
 */
int ch_set_ctrl(struct ch_device *device, struct ch_ctrl *ctrl, int32_t value);

/**
 * @brief Sets device format and framesize based on contents of provided struct.
 *
 * @param device Device to set with parameters.
 * @return 0 on success, -1 on failure.
 */
int ch_set_fmt(struct ch_device *device);

/**
 * @brief Initialize stream buffers.
 *
 * @param device Device to initialize.
 * @return 0 on success, -1 on failure.
 */
int ch_init_stream(struct ch_device *device);

/**
 * @brief Begin streaming from device.
 *
 * @param device Device to begin streaming from.
 * @return 0 on success, -1 on failure.
 */
int ch_start_stream(struct ch_device *device);

/**
 * @brief Stop streaming from a device.
 *
 * @param device Device to stop streaming from.
 * @return 0 on succes, -1 on failure.
 */
int ch_stop_stream(struct ch_device *device);

/**
 * @brief Stream video and call a callback upon every new frame.
 *
 * @param device Device to stream from.
 * @param num_frames Number of frames to stream. 0 for unlimited.
 * @param callback Function to callback on each new frame.
 * @return 0 on success, -1 on failure.
 */
int ch_stream(struct ch_device *device, uint32_t num_frames,
              int (*callback)(struct ch_frmbuf *frm));

/**
 * @brief Stream video and call a callback upon every new frame asynchronously.
 *        Runs in a seperate thread instance.
 *
 * @param device Device to stream from.
 * @param num_frames Number of frames to stream. 0 for unlimited.
 * @param callback Function to callback on each new frame.
 * @return 0 on success, -1 on failure.
 */
int ch_stream_async(struct ch_device *device, uint32_t n_frames,
                    int (*callback)(struct ch_frmbuf *frm));

/**
 * @brief Stops a running asynchronous stream and joins the thread.
 *
 * @param device Device currently streaming.
 * @return 0 on success, -1 on failure.
 */
int ch_stream_async_join(struct ch_device *device);

#ifdef __cplusplus
}
#endif

#endif
