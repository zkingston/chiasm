#include <stdlib.h>
#include <dlfcn.h>

#include <chiasm.h>


struct ch_dl *
ch_dl_load(const char *name)
{
    struct ch_dl *plugin = (struct ch_dl *) ch_calloc(1, sizeof(struct ch_dl));
    if (plugin == NULL)
	return (NULL);

    plugin->so = dlopen(name, RTLD_NOW);

    if (plugin->so == NULL) {
            ch_error("Failed to open dynamic library.");
            ch_error(dlerror());
            free(plugin);

            return (NULL);
    }

    dlerror();

    // Load all functions. None are required.
    plugin->init = (int (*)(struct ch_device *, struct ch_dl_cx *))
        dlsym(plugin->so, CH_STR(CH_DL_INIT));

    plugin->callback =
	(int (*)(struct ch_frmbuf *)) dlsym(plugin->so, CH_STR(CH_DL_CALL));

    plugin->quit =
	(int (*)(void)) dlsym(plugin->so, CH_STR(CH_DL_QUIT));

    // Initialize plugin context.
    size_t idx;
    for (idx = 0; idx < CH_DL_NUMBUF; idx++) {
        plugin->cx.out_buffer[idx].start = NULL;
        plugin->cx.out_buffer[idx].length = 0;
        plugin->cx.nonce[idx] = 0;
    }

    plugin->cx.thread = 0;
    pthread_mutex_init(&plugin->cx.mutex, NULL);
    pthread_cond_init(&plugin->cx.cond, NULL);
    plugin->cx.active = false;

    plugin->cx.select = 0;
    plugin->cx.b_per_pix = 0;
    plugin->cx.out_pixfmt = CH_DEFAULT_OUTFMT;
    plugin->cx.out_stride = 0;
    plugin->cx.sws_cx = NULL;
    plugin->cx.frame_out = NULL;
    plugin->cx.undistort = false;

    return (plugin);
}

void
ch_dl_close(struct ch_dl *plugin)
{
    dlclose(plugin->so);
    free(plugin);
}

struct ch_plugin_thread_args {
    struct ch_dl *plugin;
    struct ch_device *device;
};

/**
 * @brief A plugin thread that waits for new frames to arrive and performs a
 *        callback.
 * @param arg The plugin and device stored inside a struct ch_plugin_thread_args.
 * @return Always NULL.
 */
void *
ch_plugin_thread(void *arg)
{
    struct ch_plugin_thread_args *args = (struct ch_plugin_thread_args *) arg;

    struct ch_device *device = args->device;
    struct ch_dl *plugin = args->plugin;
    struct ch_dl_cx *cx = &plugin->cx;

    free(args);

    uint32_t nonce = cx->nonce[cx->select];

    while (cx->active) {
        pthread_mutex_lock(&cx->mutex);

        uint32_t idx = (cx->select + 1) % CH_DL_NUMBUF;
        if (cx->nonce[idx] <= nonce)
            pthread_cond_wait(&cx->cond, &cx->mutex);

        if (!cx->active)
            break;

        nonce = cx->nonce[idx];
        cx->select = idx;

        pthread_mutex_unlock(&cx->mutex);

        if (device->calib && cx->undistort)
            ch_undistort(device, cx, &cx->out_buffer[cx->select]);

        if (plugin->callback(&cx->out_buffer[cx->select]) == -1)
            cx->active = false;
    }

    return (NULL);
}

/**
 * @brief Create a plugin thread.
 *
 * @param plugin The plugin to create the thread for.
 * @return 0 on success, -1 on failure.
 */
static int
ch_create_plugin_thread(struct ch_device *device, struct ch_dl *plugin)
{
    struct ch_dl_cx *cx = &plugin->cx;
    cx->active = true;

    struct ch_plugin_thread_args *args = (struct ch_plugin_thread_args *)
        ch_calloc(1, sizeof(struct ch_plugin_thread_args));

    if (args == NULL)
        return (-1);

    args->device = device;
    args->plugin = plugin;

    if (ch_start_thread(&cx->thread, NULL, ch_plugin_thread, args) == -1)
        return (-1);

    return (0);
}

/**
 * @brief Join a plugin thread.
 *
 * @param plugin Plugin to stop and join.
 * @return 0 on success, -1 on failure.
 */
static int
ch_join_plugin_thread(struct ch_dl *plugin)
{
    plugin->cx.active = false;
    pthread_cond_signal(&plugin->cx.cond);

    if (ch_join_thread(plugin->cx.thread, NULL) == -1)
        return (-1);

    return (0);
}

int
ch_init_plugins(struct ch_device *device, struct ch_dl *plugins[], size_t n_plugins)
{
    size_t idx;
    for (idx = 0; idx < n_plugins; idx++) {
        if (plugins[idx]->init(device, &plugins[idx]->cx) == -1) {
            ch_error("Failed to initialize plugin.");
            ch_quit_plugins(plugins, idx);
            return (-1);
        }

        // Initialize output context for plugin.
        if (ch_init_plugin_out(device, &plugins[idx]->cx) == -1)
            return (-1);

        if (ch_create_plugin_thread(device, plugins[idx]) == -1)
            return (-1);
    }

    return (0);
}

int
ch_update_plugins(struct ch_device *device, struct ch_decode_cx *decode,
                struct ch_dl *plugins[], size_t n_plugins)
{
    size_t idx;
    for (idx = 0; idx < n_plugins; idx++) {
        struct ch_dl_cx *cx = &plugins[idx]->cx;

        if (!cx->active)
            return (-1);

        pthread_mutex_lock(&cx->mutex);

        // Should output into the next buffer (select + 1 % NUM)
        if (ch_output(device, decode, &plugins[idx]->cx) == -1)
            return (-1);

        pthread_mutex_unlock(&cx->mutex);
        pthread_cond_signal(&cx->cond);
    }

    return (0);
}

int
ch_quit_plugins(struct ch_dl *plugins[], size_t n_plugins)
{
    size_t idx;
    for (idx = 0; idx < n_plugins; idx++) {
        if (ch_join_plugin_thread(plugins[idx]) == -1)
            ch_error("Failed to join plugin thread.");

        if (plugins[idx]->quit() == -1)
            ch_error("Failed to close plugin.");

        ch_destroy_plugin_out(&plugins[idx]->cx);
    }

    return (0);
}
