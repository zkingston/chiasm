#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <gtk/gtk.h>
#include <libavcodec/avcodec.h>

#include <chiasm.h>

pthread_t gui_thread;

/**
 * @brief Callback function that occurs on a timer. Used for repaint.
 */
static gboolean
timer_callback(GtkWidget *widget)
{
    gtk_widget_queue_draw(widget);

    return (TRUE);
}

/**
 * @brief Callback function for expose events. Draws the new image.
 */
static gboolean
on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    struct ch_device *device = (struct ch_device *) data;

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);

    if (!device->stream)
	return (FALSE);

    int x;
    for (x = 0; x < device->framesize.width; x++) {
        int y;
        for (y = 0; y < device->framesize.height; y++) {
            cairo_rectangle(cr, x, y, 1, 1);


            double r, g, b;
            if (device->decode_cx.out_pixfmt == AV_PIX_FMT_RGB24) {
                uint8_t *buf = &device->out_buffer.start[3 * device->out_stride * y];
                int o = x * 3;

                r = buf[o + 0] / 255.0;
                g = buf[o + 1] / 255.0;
                b = buf[o + 2] / 255.0;

            } else {
                uint8_t *buf = &device->out_buffer.start[device->out_stride * y];
                int o = x;

                r = buf[o + 0] / 255.0;
                g = buf[o + 0] / 255.0;
                b = buf[o + 0] / 255.0;
            }

            cairo_set_source_rgb(cr, r, g, b);
            cairo_fill(cr);
        }
    }

    return (FALSE);
}

/**
 * @brief Initialize and start the GTK mainloop.
 */
static void *
setup_gui(void *arg)
{
    struct ch_device *device = (struct ch_device *) arg;

    gtk_init(0, NULL);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *drawRGB = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), drawRGB);

    gtk_window_set_default_size(GTK_WINDOW(window),
				device->framesize.width, device->framesize.height);

    // Register destroy signal.
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    // Register repaint signal.
    g_signal_connect(G_OBJECT(drawRGB), "draw",
                     G_CALLBACK(on_draw_event), device);

    // Register timer event.
    g_timeout_add(ch_get_fps(device), (GSourceFunc) timer_callback, (gpointer) drawRGB);
    timer_callback(drawRGB);

    // Show window and begin mainloop.
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_show_all(window);

    gtk_main();

    return (NULL);
}

int
CH_DL_INIT(struct ch_device *device)
{
    // Create display thread.
    int r;
    if ((r = pthread_create(&gui_thread, NULL, setup_gui, device)) != 0) {
	ch_error_no("Failed to create display thread.", r);
	return (-1);
    }

    return (0);
}

int
CH_DL_QUIT(struct ch_device *device)
{
    device = (struct ch_device *) device;

    // Close display thread and join.
    gtk_main_quit();

    int r;
    if ((r = pthread_join(gui_thread, NULL)) != 0) {
	ch_error_no("Failed to join display thread.", r);
	return (-1);
    }

    gui_thread = 0;
    return (0);
}
