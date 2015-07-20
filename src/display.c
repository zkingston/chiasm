#include <stdio.h>
#include <stdlib.h>

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
    if (buf == NULL)
	return (FALSE);

    gdk_draw_rgb_image(widget->window,
		       widget->style->fg_gc[GTK_STATE_NORMAL],
		       0,
		       0,
		       device.framesize.width,
		       device.framesize.height,
		       GDK_RGB_DITHER_MAX,
		       buf->start,
		       device.framesize.width * 3);


    return (TRUE);
}

// TODO: Parse command-line args

int
main(int argc, char *argv[])
{
    int n_frames = 0;

    // Just use defaults for now...
    ch_init_device(&device);

    // Super hacky. Will do error handling gracefully later.
    if (ch_open_device(&device) == -1)
	return (-1);

    if (ch_set_fmt(&device) == -1)
	goto cleanup;

    if (ch_stream_async(&device, n_frames, stream_callback) == -1)
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

    return (0);

cleanup:
    ch_stream_async_join(&device);
    ch_close_device(&device);

    return (-1);
}
