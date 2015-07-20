#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <chiasm.h>

struct ch_device device;
struct ch_frmbuf *buf;

static int
stream_callback(struct ch_frmbuf *frm)
{
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

static gboolean
on_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    return (TRUE);
}

int
main(int argc, char *argv[])
{
    int n_frames = 0;
    ch_init_device(&device);

    // Super hacky.
    if (ch_open_device(&device) == -1)
        exit(-1);

    if (ch_set_fmt(&device) == -1)
        exit(-1);

    if (ch_stream_async(&device, n_frames, stream_callback) == -1)
        exit(-1);

    // Basic setup.
    gtk_init(&argc, &argv);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    // Register destroy signal.
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    // Register repaint signal.
    g_signal_connect(G_OBJECT(window), "expose-event",
                     G_CALLBACK(on_expose_event), NULL);

    // Register timer event.
    g_timeout_add(33, (GSourceFunc) timer_callback, (gpointer) window);
    timer_callback(window);

    // Show window and begin mainloop.
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_show_all(window);

    gtk_main();

    return (0);
}
