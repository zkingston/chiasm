#ifndef CHIASM_PLUGIN_H_
#define CHIASM_PLUGIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <chiasm/types.h>

/**
 * @brief Loads a chiasm plugin.
 *
 * @param name Name of plugin to load.
 * @return Allocated plugin description struct ch_dl.
 */
struct ch_dl *ch_dl_load(const char *name);

/**
 * @brief Closes a chiasm plugin. Frees allocated memory.
 *
 * @param plugin Allocated struct ch_dl loaded by ch_dl_load.
 * @return None.
 */
void ch_dl_close(struct ch_dl *plugin);

/**
 * @brief Initialize an array of plugins with a device.
 *
 * @param device Device to use in initialization.
 * @param plugins An array of loaded plugins.
 * @param n_plugins Number of plugins in the array.
 * @return 0 on success, -1 on failure.
 */
int ch_init_plugins(struct ch_device *device, struct ch_dl **plugins, size_t n_plugins);

/**
 * @brief Update image for an array of plugins.
 *
 * @param device The device the plugins are associated with.
 * @param decode The decoding context used for stream decompression.
 * @param plugins The array of plugins.
 * @param n_plugins The number of plugins in the array.
 * @return 0 on success, -1 on failure.
 */
int ch_update_plugins(struct ch_device *device, struct ch_decode_cx *decode,
                    struct ch_dl *plugins[], size_t n_plugins);

/**
 * @brief Exit out of all plugins in an array.
 *
 * @param plugins An array of initialized plugins.
 * @param n_plugins Number of plugins in the array.
 * @return 0 on success, -1 on failure.
 */
int ch_quit_plugins(struct ch_dl **plugins, size_t n_plugins);

// Plugin functions that need to be implemented.
#define CH_DL_INIT ch_dl_init
#define CH_DL_CALL ch_dl_callback
#define CH_DL_QUIT ch_dl_quit

/**
 * @brief Plugin initialization function. Use this to set up any state needed.
 *        Not required.
 *
 * @return 0 on success, -1 on failure.
 */
int CH_DL_INIT(struct ch_device *, struct ch_dl_cx *);

/**
 * @brief Plugin callback function. Called on every new frame available from device.
 *        Output buffer mutex has lock when called. Not required.
 *
 * @return 0 on success, -1 on failure.
 */
int CH_DL_CALL(struct ch_frmbuf *);

/**
 * @brief Plugin function to be called on close to clean up. Not required.
 *
 * @return 0 on succes, -1 on failure.
 */
int CH_DL_QUIT(void);

#ifdef __cplusplus
}
#endif

#endif
