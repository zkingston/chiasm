#include <stdlib.h>
#include <dlfcn.h>

#include <chiasm.h>

struct ch_dl *
ch_dl_load(const char *name)
{
    struct ch_dl *plugin = ch_calloc(1, sizeof(struct ch_dl));
    if (plugin == NULL)
	return (NULL);

    plugin->so = dlopen(name, RTLD_NOW);

    if (plugin->so == NULL) {
	ch_error("Failed to open dynamic library.");
	ch_error(dlerror());
	free(plugin);

	return (NULL);
    }

    // Load all functions. None are required.
    plugin->init = (int (*)(struct ch_device *, struct ch_dl_cx *))
        dlsym(plugin->so, CH_STR(CH_DL_INIT));

    plugin->callback =
	(int (*)(struct ch_frmbuf *)) dlsym(plugin->so, CH_STR(CH_DL_CALL));

    plugin->quit =
	(int (*)(void)) dlsym(plugin->so, CH_STR(CH_DL_QUIT));

    // Initialize plugin context.
    plugin->cx.out_buffer.start = NULL;
    plugin->cx.out_buffer.length = 0;
    plugin->cx.out_pixfmt = CH_DEFAULT_OUTFMT;
    plugin->cx.out_stride = 0;
    plugin->cx.sws_cx = NULL;
    plugin->cx.frame_out = NULL;

    return (plugin);
}

void
ch_dl_close(struct ch_dl *plugin)
{
    dlclose(plugin->so);
    free(plugin);
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
        ch_init_plugin_out(device, &plugins[idx]->cx);
    }

    return (0);
}

int
ch_call_plugins(struct ch_device *device, struct ch_decode_cx *decode,
                struct ch_dl *plugins[], size_t n_plugins)
{
    size_t idx;
    for (idx = 0; idx < n_plugins; idx++) {
        if (ch_output(device, decode, &plugins[idx]->cx) == -1)
            return (-1);

        if (plugins[idx]->callback(&plugins[idx]->cx.out_buffer) == -1)
            return (-1);
    }

    return (0);
}

int
ch_quit_plugins(struct ch_dl *plugins[], size_t n_plugins)
{
    size_t idx;
    for (idx = 0; idx < n_plugins; idx++)
        if (plugins[idx]->quit() == -1)
            ch_error("Failed to close plugin.");

    return (0);
}
