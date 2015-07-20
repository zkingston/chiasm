#include <gtk/gtk.h>

int
main(int argc, char *argv[])
{
    // Make glib thread-safe.
    if (!g_thread_supported())
        g_thread_init(NULL);

    // Set up global mutex and obtain.
    gdk_threads_init();
    gdk_threads_enter();

    // Basic setup.
    gtk_init(&argc, &argv);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    // Start application.
    gtk_widget_show_all(window);
    gtk_main();

    // Release global mutex.
    gdk_threads_leave();

    return (0);
}
