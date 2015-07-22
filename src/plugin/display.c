#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <gtk/gtk.h>

#include <chiasm.h>

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
    event = (GdkEventExpose *) event;
    struct ch_device *device = (struct ch_device *) data;

    if (!device->stream)
	return (FALSE);

    pthread_mutex_lock(&device->out_mutex);

    gdk_draw_rgb_image(
	widget->window,
	widget->style->fg_gc[GTK_STATE_NORMAL],
	0,
	0,
	device->framesize.width,
	device->framesize.height,
	GDK_RGB_DITHER_MAX,
	device->out_buffer.start,
	device->framesize.width * 3
    );

    pthread_mutex_unlock(&device->out_mutex);

    return (TRUE);
}

static void *
setup_gui(void *arg)
{
    struct ch_device *device = (struct ch_device *) arg;

    gtk_init(0, NULL);
    gdk_rgb_init();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *drawRGB = gtk_drawing_area_new();
    gtk_drawing_area_size(GTK_DRAWING_AREA(drawRGB),
			  device->framesize.width, device->framesize.height);

    gtk_container_add(GTK_CONTAINER(window), drawRGB);

    gtk_window_set_default_size(GTK_WINDOW(window),
				device->framesize.width, device->framesize.height);

    // Register destroy signal.
    // TODO: Have destroy signal go into handler to shutdown webcam.
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    // Register repaint signal.
    g_signal_connect(G_OBJECT(drawRGB), "expose-event",
                     G_CALLBACK(on_expose_event), device);

    // TODO: obtain framerate from device.
    // Register timer event.
    g_timeout_add(33, (GSourceFunc) timer_callback, (gpointer) window);
    timer_callback(window);

    // Show window and begin mainloop.
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_show_all(window);

    gtk_main();
}

int
CH_DL_INIT(struct ch_device *device)
{
    pthread_t t;

    int r;
    if ((r = pthread_create(&t, NULL, setup_gui, device)) != 0) {
	ch_error_no("Failed to create display thread.", r);
	return (-1);
    }

    return (0);
}

int
CH_DL_CALL(struct ch_device *device)
{
    return (0);
}
