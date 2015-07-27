#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <gtk/gtk.h>
#include <libavcodec/avcodec.h>

#include <chiasm.h>

pthread_t gui_thread;
struct ch_frmbuf outbuf;

/**
 * @brief Callback function that occurs on a timer. Used for repaint.
 */
static gboolean
timer_callback(GtkWidget *widget)
{
    gtk_widget_queue_draw(widget);

    return (TRUE);
}

static int
convert_image(struct ch_device *device, struct ch_frmbuf *out) {
    size_t x;
    for (x = 0; x < device->framesize.width; x++) {
        size_t y;
        for (y = 0; y < device->framesize.height; y++) {
            uint8_t *buf;
            uint8_t r, g, b;
            switch (device->decode_cx.out_pixfmt) {
            case AV_PIX_FMT_RGB24:
                buf = &device->out_buffer.start[3 * device->out_stride * y];

                r = buf[x * 3 + 0];
                g = buf[x * 3 + 1];
                b = buf[x * 3 + 2];
                break;

            case AV_PIX_FMT_GRAY8:
                buf = &device->out_buffer.start[device->out_stride * y];
                r = g = b = buf[x];
                break;

            default:
                ch_error("Invalid image type for display.");
                return (-1);
            }

            uint8_t *out_buf = &out->start[4 * device->framesize.width * y];
            out_buf[x * 4 + 2] = r;
            out_buf[x * 4 + 1] = g;
            out_buf[x * 4 + 0] = b;
        }
    }

    return (0);
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

    if (convert_image(device, &outbuf) == -1)
        return (FALSE);

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
                                               device->framesize.width);

    cairo_surface_t *image = cairo_image_surface_create_for_data(
        outbuf.start,
        CAIRO_FORMAT_RGB24,
        device->framesize.width,
        device->framesize.height,
        stride
    );

    cairo_set_source_surface(cr, image, 0, 0);
    cairo_paint(cr);

    cairo_surface_destroy(image);

    cairo_select_font_face(cr, "Sans",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);

    char buf[100];
    sprintf(buf, "FPS: %3.2f", device->fps);

    cairo_set_font_size(cr, 18);

    cairo_text_extents_t extents;
    cairo_text_extents(cr, buf, &extents);


    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_move_to(cr, 10, 18);
    cairo_show_text(cr, buf);

    return (FALSE);
}

/**
 * @brief Initialize and start the GTK mainloop.
 */
static void *
setup_gui(void *arg)
{
    struct ch_device *device = (struct ch_device *) arg;

    // Create output buffer for Cairo graphics.
    outbuf.length = 4 * device->framesize.width * device->framesize.height;
    outbuf.start =
        ch_calloc(1, outbuf.length);

    gtk_init(0, NULL);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    // Set up drawing area.
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
