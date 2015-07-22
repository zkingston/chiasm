#ifndef CHIASM_PLUGIN_H_
#define CHIASM_PLUGIN_H_

#ifdef __cplusplus
extern "C" {
#endif

struct ch_dl {
    void *so;
    int (*init)(struct ch_device *device);
    int (*callback)(struct ch_device *device);
    int (*quit)(struct ch_device *device);
};

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

// Plugin functions that need to be implemented.
#define CH_DL_INIT ch_dl_init
#define CH_DL_CALL ch_dl_callback
#define CH_DL_QUIT ch_dl_quit

/**
 * @brief Plugin initialization function. Use this to set up any state needed.
 *        Not required.
 *
 * @param device Device that the callback will be issued over.
 * @return 0 on success, -1 on failure.
 */
int CH_DL_INIT(struct ch_device *device);

/**
 * @brief Plugin callback function. Called on every new frame available from device.
 *        Output buffer mutex has lock when called. Not required.
 *
 * @param device Device the callback is for.
 * @return 0 on success, -1 on failure.
 */
int CH_DL_CALL(struct ch_device *device);

/**
 * @brief Plugin function to be called on close to clean up. Not required.
 *
 * @param device Device for context information if needed.
 * @return 0 on succes, -1 on failure.
 */
int CH_DL_QUIT(struct ch_device *device);

#ifdef __cplusplus
}
#endif

#endif
