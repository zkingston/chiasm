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

#define CH_DL_NUMBUF 2

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
 * @brief Description of a frame-interval in fractions of a second.
 */
struct ch_frmival {
    uint32_t numerator;
    uint32_t denominator;
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
 * @brief A description of a video device and all associated context.
 */
struct ch_device {
    char             *name;       /**< Filename of the device. */
    int              fd;          /**< File-descriptor of the device. */
    pthread_mutex_t  mutex;

    struct ch_frmbuf *in_buffers; /**< Array of memory-mapped input buffers. */
    uint32_t         num_buffers; /**< Number of input buffers. */

    struct ch_rect   framesize;   /**< Size of frames in image stream. */
    uint32_t         in_pixfmt;   /**< Format of incoming pixels from stream.
				        V4L pixelformat. */

    struct timeval   timeout;     /**< Timeout on select to get new image. */
    bool             stream;      /**< Is the device currently streaming? */
    double           fps;         /**< Current framerate of the device. */
};

/**
 * @brief Decoding context for compressed image decoding.
 */
struct ch_decode_cx {
    AVCodecContext *codec_cx;   /**< libavcodec codec context. */
    AVFrame        *frame_in;   /**< Allocated input frame. */
    enum AVPixelFormat        in_pixfmt;  /**< Decoded output pixel format. */
};

/**
 * @brief Plugin output image format context.
 */
struct ch_dl_cx {
    struct ch_frmbuf   out_buffer[CH_DL_NUMBUF]; /**< Output buffers. */
    uint64_t           nonce[CH_DL_NUMBUF];      /**< Output buffer nonce. */
    uint32_t           select;     /**< Which buffer are we currently using? */

    pthread_t          thread;     /**< Thread ID for plugin. */
    pthread_mutex_t    mutex;      /**< Mutex for thread producer / consumer. */
    pthread_cond_t     cond;       /**< Condition variable for thread. */
    bool               active;     /**< Is the plugin active? */

    enum AVPixelFormat out_pixfmt; /**< Output pixel format. */
    uint32_t           b_per_pix;  /**< Bytes per pixel in output format. */
    uint32_t           out_stride; /**< Stride of the output image. */
    struct SwsContext  *sws_cx;    /**< SWS context for decoding. */
    AVFrame            *frame_out; /**< Allocated output frame. */
};

/**
 * @brief Plugin description that has been dynamically loaded.
 */
struct ch_dl {
    char *name;                          /**< Name of plugin. */
    void *so;                            /**< Shared object from dlopen. */
    int (*init)(struct ch_device *,
                struct ch_dl_cx *);      /**< Initializer function. */
    int (*callback)(struct ch_frmbuf *); /**< Frame callback function. */
    int (*quit)(void);                   /**< Destroyer function. */
    struct ch_dl_cx cx;                  /**< Decoding context for plugin. */
};

#ifdef __cplusplus
}
#endif

#endif
