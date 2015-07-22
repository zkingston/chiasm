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
    plugin->init =
	(int (*)(struct ch_device *)) dlsym(plugin->so, CH_STR(CH_DL_INIT));

    plugin->callback =
	(int (*)(struct ch_device *)) dlsym(plugin->so, CH_STR(CH_DL_CALL));

    plugin->quit =
	(int (*)(struct ch_device *)) dlsym(plugin->so, CH_STR(CH_DL_QUIT));

    return (plugin);
}


void
ch_dl_close(struct ch_dl *plugin)
{
    dlclose(plugin->so);
    free(plugin);
}
