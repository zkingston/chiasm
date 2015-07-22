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

    char *err = NULL;

    plugin->init =
	(int (*)(struct ch_device *)) dlsym(plugin->so, CH_STR(CH_DL_INIT));
    err = dlerror();

    if (err) {
	ch_error("Failed to load plugin init function.");
	ch_error(err);

	goto clean;
    }

    plugin->callback =
	(int (*)(struct ch_device *)) dlsym(plugin->so, CH_STR(CH_DL_CALL));
    err = dlerror();

    if (err) {
	ch_error("Failed to load plugin callback function.");
	ch_error(err);

	goto clean;
    }

    return (plugin);

clean:
    ch_dl_close(plugin);
    return (NULL);
}


void
ch_dl_close(struct ch_dl *plugin)
{
    dlclose(plugin->so);
    free(plugin);
}
