#include "f1sh_camera_rx.h"

static void on_window_destroy(GtkWidget *widget __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    app_cleanup(app);
    gtk_main_quit();
}

void on_widget_destroy(GtkWidget *widget __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) {
        ui_log(app, "Error: app is NULL in widget destroy");
        return;
    }
    ui_log(app, "Closed Widget");
    gtk_widget_destroy(widget);
}

// Serial scan helper types and functions (file-scope)
typedef struct { App *app; char *port; } SerialUpdate;

static gboolean update_serial_label_idle(gpointer data) {
    SerialUpdate *u = (SerialUpdate*)data;
    if (u && u->app && u->app->serial_status_label) {
        if (u->port) {
            char buf[256];
            snprintf(buf, sizeof(buf), "COM port: %s", u->port);
            gtk_label_set_text(GTK_LABEL(u->app->serial_status_label), buf);
            ui_log(u->app, "Camera connected via COM port: %s", u->port);
        } else {
            gtk_label_set_text(GTK_LABEL(u->app->serial_status_label), "No camera connected via COM port");
            ui_log(u->app, "No camera connected via COM port");
        }
    }
    if (u) {
        if (u->port) g_free(u->port);
        g_free(u);
    }
    return FALSE;
}

static gpointer serial_scan_thread(gpointer user_data) {
    App *a = (App*)user_data;
    char *port = serial_find_camera_port(a);
    SerialUpdate *u = g_new0(SerialUpdate, 1);
    u->app = a;
    u->port = port; // may be NULL
    g_idle_add(update_serial_label_idle, u);
    return NULL;
}

static void on_rotate_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) {
        ui_log(app, "Error: app is NULL in rotate");
        return;
    }
    if (app->streaming) {
        on_open_stream_clicked(NULL, app);
        return;
    }
    ui_rotate(app);
    ui_log(app, "Opened rotate settings");
}

static void on_config_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) {
        ui_log(app, "Error: app is NULL in configuration");
        return;
    }
    ui_login(app);
    ui_log(app, "Opened login window");
}

static void on_clear_log_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app || !app->log_view) return;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->log_view));
    if (buffer) {
        gtk_text_buffer_set_text(buffer, "", -1);
    }
}

void ui_update_status(App *app, const char *status) {
    if (app->config_status_label) {
        gtk_label_set_text(GTK_LABEL(app->config_status_label), status);
    }
    ui_log(app, "Status: %s", status);
}

void ui_main(App *app) {
    // Main window
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "F1sh Camera RX");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(app->window), GTK_WIN_POS_CENTER);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_window_destroy), app);

    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(app->window), main_box);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);

    // Log box
    GtkWidget *log_frame = gtk_frame_new("Application Log");
    gtk_widget_set_margin_start(log_frame, 8);
    gtk_widget_set_margin_end(log_frame, 8);
    gtk_widget_set_margin_top(log_frame, 8);
    gtk_widget_set_margin_bottom(log_frame, 8);
    gtk_box_pack_start(GTK_BOX(main_box), log_frame, TRUE, TRUE, 0);

    GtkWidget *log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(log_view), FALSE);
    gtk_widget_set_can_focus(log_view, FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(log_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(log_view), 2);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(log_view), 2);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(log_view), 4);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(log_view), 4);
    
    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_widget_set_hexpand(log_scroll, TRUE);
    gtk_widget_set_vexpand(log_scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(log_scroll), log_view);
    gtk_widget_set_margin_start(log_scroll, 6);
    gtk_widget_set_margin_end(log_scroll, 6);
    gtk_widget_set_margin_top(log_scroll, 6);
    gtk_widget_set_margin_bottom(log_scroll, 6);
    gtk_container_add(GTK_CONTAINER(log_frame), log_scroll);
    app->log_view = log_view;
    
    GtkCssProvider *css = gtk_css_provider_new();
    const char *css_data = ".log-scroll { border-radius: 10px; padding: 3px; background-color: #ffffffff; } .log-text { font-family: consolas; font-size: 9pt; }";
    gtk_css_provider_load_from_data(css, css_data, -1, NULL);
    GdkScreen *screen = gtk_widget_get_screen(app->window);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_style_context_add_class(gtk_widget_get_style_context(log_scroll), "log-scroll");
    gtk_style_context_add_class(gtk_widget_get_style_context(log_view), "log-text");
    g_object_unref(css);

    // Flush any logs buffered before UI creation so early messages appear in the log
    ui_log_flush_buffer(app);
    ui_set_log_interactive(app, app->log_interactive);

    // Camera connection status
    if (!http_test_connection(app)) {
        app->camera_status = gtk_label_new("No camera connected wirelessly");
        gtk_box_pack_start(GTK_BOX(main_box), app->camera_status, FALSE, FALSE, 0);
    } else {
        app->camera_status = gtk_label_new("Connected to camera wirelessly");
        gtk_box_pack_start(GTK_BOX(main_box), app->camera_status, FALSE, FALSE, 0);
    }

    // Serial port status (will be updated asynchronously)
    app->serial_status_label = gtk_label_new("Searching for camera...");
    gtk_box_pack_start(GTK_BOX(main_box), app->serial_status_label, FALSE, FALSE, 0);

    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 10);

    // Setup button
    GtkWidget *connect_btn = gtk_button_new_with_label("Setup camera");
    // g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_connect_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), connect_btn, FALSE, FALSE, 0);

    // Stream button
    GtkWidget *stream_btn = gtk_button_new_with_label("Open Stream");
    app->stream_button_main = stream_btn;
    if (app->streaming) {
        gtk_button_set_label(GTK_BUTTON(stream_btn), "Stop Stream");
    }
    g_signal_connect(stream_btn, "clicked", G_CALLBACK(on_rotate_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), stream_btn, FALSE, FALSE, 0);


    // Configuration button
    GtkWidget *config_btn = gtk_button_new_with_label("Advanced Configuration");
    g_signal_connect(config_btn, "clicked", G_CALLBACK(on_config_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), config_btn, FALSE, FALSE, 0);

    // Clear log button
    GtkWidget *clear_btn = gtk_button_new_with_label("Clear Log");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_log_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), clear_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(app->window);

    // Start background scan for camera serial port
    g_thread_new("serial-scan", serial_scan_thread, app);
}
