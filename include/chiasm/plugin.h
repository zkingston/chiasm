#ifndef CHIASM_PLUGIN_H_
#define CHIASM_PLUGIN_H_

struct ch_dl {
    void *so;
    int (*init)(struct ch_device *device);
    int (*callback)(struct ch_device *device);
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

int CH_DL_INIT(struct ch_device *device);
int CH_DL_CALL(struct ch_device *device);

#endif
