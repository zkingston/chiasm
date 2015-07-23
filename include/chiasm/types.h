#ifndef CHIASM_TYPES_H_
#define CHIASM_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

/**
 * @brief Simple struct to describe a rectangle.
 */
struct ch_rect {
    uint32_t width;  /**< Width dimension. */
    uint32_t height; /**< Height dimension. */
};

/**
 * @brief Container for an allocated array of image pixelformats.
 */
struct ch_fmts {
    uint32_t *fmts;   /**< Array of pixelformats. */
    uint32_t  length; /**< Length of the array. */
};

/**
 * @brief Container for an allocated array of image framesizes.
 */
struct ch_frmsizes {
    struct ch_rect *frmsizes; /**< Array of framesizes. */
    uint32_t        length;   /**< Length of the array. */
};

/**
 * @brief Union describing an element of a device control menu item.
 */
union ch_ctrl_menu_item {
    char    name[32]; /**< The name of the item. */
    int64_t value;    /**< The value of the item. */
};

/**
 * @brief Container for an allocated array of device control menu items.
 */
struct ch_ctrl_menu {
    union ch_ctrl_menu_item *items;  /**< Array of items. */
    uint32_t                 length; /**<  Length of the array. */
};

/**
 * @brief Local description of a device control.
 */
struct ch_ctrl {
    uint32_t id;       /**< The ID of the control. */
    char     name[32]; /**< Description of the control. */
    uint32_t type;     /**< Control type. */
    int32_t  min;      /**< Mininum value of the control. */
    int32_t  max;      /**< Maximum value of the control. */
    int32_t  step;     /**< Step value of the range of the control. */
    int32_t  defval;   /**< Default value of the control. */
};

/**
 * @brief Container for an allocated array of control descriptions.
 */
struct ch_ctrls {
    struct ch_ctrl *ctrls; /**< Array of controls. */
    uint32_t length;       /**< Lenght of the array. */
};

/**
 * @brief A framebuffer for image frame I/O.
 */
struct ch_frmbuf {
    uint8_t  *start;  /**< Start of framebuffer array. */
    uint32_t  length; /**< Length of the array. */
};

/**
 * @brief Decoding context to use in frame decoding for compressed streams.
 */
struct ch_decode_cx {
    AVCodecContext *codec_cx;  /**< libavcodec codec context. */
    AVFrame *frame_in;         /**< Allocated input frame. */
    AVFrame *frame_out;        /**< Allocated output frame. */
    struct SwsContext *sws_cx; /**< SWS context. */
    bool compressed;           /**< Is this a compressed stream? */
};

/**
 * @brief A description of a video device and all associated context.
 */
struct ch_device {
    char *name;                    /**< Filename of the device. */
    int   fd;                      /**< File-descriptor of the device. */

    struct ch_frmbuf *in_buffers;  /**< Array of memory-mapped input buffers. */
    uint32_t          num_buffers; /**< Number of input buffers. */

    struct ch_frmbuf *in_buffer;   /**< The current input buffer. */

    struct ch_frmbuf out_buffer;   /**< Output buffer. Contains RGB24 image. */
    pthread_mutex_t  out_mutex;    /**< Mutex to lock device buffers. */

    struct ch_rect framesize;      /**< Size of frames in image stream. */
    uint32_t       pixelformat;    /**< Format of incoming pixels from stream. */

    struct timeval timeout;        /**< Timeout on select to get new image. */
    bool stream;                   /**< Is the device currently streaming? */

    pthread_t thread;              /**< Thread ID for asynchronous streaming. */

    struct ch_decode_cx decode_cx; /**< Decoding context for compressed video. */
};

/**
 * @brief Argument for thread in asynchronous streaming.
 */
struct ch_stream_args {
    struct ch_device *device;                  /**< Device to use. */
    uint32_t n_frames;                         /**< Number of frames. */
    int (*callback)(struct ch_device *device); /**< Callback function. */
};

/**
 * @brief Plugin description that has been dynamically loaded.
 */
struct ch_dl {
    void *so;                                  /**< Shared object from dlopen. */
    int (*init)(struct ch_device *device);     /**< Initializer function. */
    int (*callback)(struct ch_device *device); /**< Frame callback function. */
    int (*quit)(struct ch_device *device);     /**< Destroyer function. */
};

#ifdef __cplusplus
}
#endif

#endif
