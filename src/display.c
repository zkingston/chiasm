#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include <chiasm.h>

struct ch_device device;
struct ch_frmbuf *buf = NULL;

static int
stream_callback(struct ch_frmbuf *frm)
{
    fprintf(stderr, ".");
    buf = frm;
    return (0);
}

static gboolean
timer_callback(GtkWidget *widget)
{
    if (widget->window == NULL)
	return (FALSE);

    gtk_widget_queue_draw(widget);

    return (TRUE);
}

// TODO: Investiage double-buffering, or some form of buffered imaging.

static gboolean
on_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    // Please stop shouting, GCC.
    event = (GdkEventExpose *) event;
    data = (gpointer) data;

    if (buf == NULL)
	return (FALSE);

    pthread_mutex_lock(&device.out_mutex);

    gdk_draw_rgb_image(
	widget->window,
	widget->style->fg_gc[GTK_STATE_NORMAL],
	0,
	0,
	device.framesize.width,
	device.framesize.height,
	GDK_RGB_DITHER_MAX,
	buf->start,
	device.framesize.width * 3
    );

    pthread_mutex_unlock(&device.out_mutex);

    return (TRUE);
}

int
main(int argc, char *argv[])
{
    int n_frames = 0;
    ch_init_device(&device);

    int opt;
    while ((opt = getopt(argc, argv, CH_OPTS "n:h?")) != -1) {
        switch (opt) {
	case 'd':
	case 't':
	case 'b':
	case 'f':
	case 'g':
	    if (ch_parse_device_opt(opt, optarg, &device) == -1)
		return (-1);

	    break;

	case 'n':
            n_frames = strtoul(optarg, NULL, 10);
            if (errno == EINVAL || errno == ERANGE) {
                fprintf(stderr, "Invalid number of frames %s.\n", optarg);
                return (-1);
            }

            break;

        case 'h':
        case '?':
        default:
            printf(
                "Usage: %s [OPTIONS]\n"
                "Options:\n"
                CH_HELP_D
		CH_HELP_F
		CH_HELP_G
		CH_HELP_B
		CH_HELP_T
                " -?,h Show this help.\n",
                argv[0]
            );

            return (0);
	}
    }

    // Super hacky. Will do error handling gracefully later.
    if (ch_open_device(&device) == -1)
	return (-1);

    int r = 0;
    if ((r = ch_set_fmt(&device)) == -1)
	goto cleanup;

    if ((r = ch_stream_async(&device, n_frames, stream_callback)) == -1)
	goto cleanup;

    // TODO: Investigate robust error handling on GUI calls.

    // Basic setup.
    gtk_init(0, NULL);
    gdk_rgb_init();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *drawRGB = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawRGB),
			  device.framesize.width, device.framesize.height);

    gtk_container_add(GTK_CONTAINER(window), drawRGB);

    gtk_window_set_default_size(GTK_WINDOW(window),
				device.framesize.width, device.framesize.height);

    // Register destroy signal.
    // TODO: Have destroy signal go into handler to shutdown webcam.
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    // Register repaint signal.
    g_signal_connect(G_OBJECT(drawRGB), "expose-event",
                     G_CALLBACK(on_expose_event), NULL);

    // TODO: obtain framerate from device.
    // Register timer event.
    g_timeout_add(33, (GSourceFunc) timer_callback, (gpointer) window);
    timer_callback(window);

    // Show window and begin mainloop.
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_show_all(window);

    gtk_main();

cleanup:
    ch_stream_async_join(&device);
    ch_close_device(&device);

    return (r);
}
