#include "f1sh_camera_rx.h"

static void on_test_connection_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in test connection\n");
        return;
    }
    
    // Safely update config from UI
    if (app->config.tx_server_ip) {
        g_free(app->config.tx_server_ip);
        app->config.tx_server_ip = NULL;
    }
    app->config.tx_server_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->tx_ip_entry)));
    app->config.tx_http_port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->tx_port_spin));
    
    if (http_test_connection(app)) {
        ui_update_status(app, "Connection OK");
    } else {
        ui_update_status(app, "Connection Failed");
    }
}

static void on_connect_clicked(GtkButton *button, gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in connect\n");
        return;
    }
    
    // Safely update all config from UI
    if (app->config.tx_server_ip) {
        g_free(app->config.tx_server_ip);
        app->config.tx_server_ip = NULL;
    }
    app->config.tx_server_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->tx_ip_entry)));
    app->config.tx_http_port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->tx_port_spin));
    
    if (app->config.rx_host_ip) {
        g_free(app->config.rx_host_ip);
        app->config.rx_host_ip = NULL;
    }
    app->config.rx_host_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->rx_ip_entry)));
    app->config.rx_stream_port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->rx_port_spin));
    
    app->config.width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->width_spin));
    app->config.height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->height_spin));
    app->config.framerate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->framerate_spin));
    app->config.rotate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->rotate_spin));
    
    // Test connection first
    if (!http_test_connection(app)) {
        ui_update_status(app, "Cannot connect to TX server");
        return;
    }
    
    // Send configuration
    if (!http_send_config(app)) {
        ui_update_status(app, "Failed to configure TX server");
        return;
    }
    
    // Also send rotate to local RX server
    if (!http_send_rx_rotate(app, app->config.rotate)) {
        ui_update_status(app, "Failed to set rotate on RX server");
        return;
    }

    ui_update_status(app, "Configuration sent to TX server");
}

static void on_open_stream_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in open stream\n");
        return;
    }
    
    if (app->connected) {
        // Stop streaming
        stream_stop(app);
        app->connected = FALSE;
        gtk_button_set_label(GTK_BUTTON(app->stream_button), "Open Stream");
        ui_update_status(app, "Stream stopped");
        return;
    }
    
    // Start streaming (assumes config was already sent)
    if (stream_start(app)) {
        app->connected = TRUE;
        gtk_button_set_label(GTK_BUTTON(app->stream_button), "Stop Stream");
        ui_update_status(app, "Streaming");
    } else {
        ui_update_status(app, "Failed to start stream");
    }
}

static void on_window_destroy(GtkWidget *widget __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    app_cleanup(app);
    gtk_main_quit();
}

void ui_create(App *app) {
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
    
    // Configuration section
    GtkWidget *config_frame = gtk_frame_new("Configuration");
    gtk_box_pack_start(GTK_BOX(main_box), config_frame, FALSE, FALSE, 0);
    
    GtkWidget *config_grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(config_frame), config_grid);
    gtk_grid_set_row_spacing(GTK_GRID(config_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(config_grid), 10);
    gtk_widget_set_margin_start(config_grid, 15);
    gtk_widget_set_margin_end(config_grid, 15);
    gtk_widget_set_margin_top(config_grid, 15);
    gtk_widget_set_margin_bottom(config_grid, 15);
    
    // TX Server settings
    int row = 0;
    GtkWidget *label = gtk_label_new("TX Server IP:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->tx_ip_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->tx_ip_entry), "127.0.0.1");
    gtk_grid_attach(GTK_GRID(config_grid), app->tx_ip_entry, 1, row, 1, 1);
    
    row++;
    label = gtk_label_new("TX Server Port:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->tx_port_spin = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->tx_port_spin), 8888);
    gtk_grid_attach(GTK_GRID(config_grid), app->tx_port_spin, 1, row, 1, 1);
    
    row++;
    label = gtk_label_new("RX Host IP:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->rx_ip_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->rx_ip_entry), "127.0.0.1");
    gtk_grid_attach(GTK_GRID(config_grid), app->rx_ip_entry, 1, row, 1, 1);
    
    row++;
    label = gtk_label_new("RX Stream Port:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->rx_port_spin = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->rx_port_spin), 5000);
    gtk_grid_attach(GTK_GRID(config_grid), app->rx_port_spin, 1, row, 1, 1);
    
    row++;
    label = gtk_label_new("Width:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->width_spin = gtk_spin_button_new_with_range(320, 4096, 8);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->width_spin), 1280);
    gtk_grid_attach(GTK_GRID(config_grid), app->width_spin, 1, row, 1, 1);
    
    row++;
    label = gtk_label_new("Height:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->height_spin = gtk_spin_button_new_with_range(240, 2160, 8);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->height_spin), 720);
    gtk_grid_attach(GTK_GRID(config_grid), app->height_spin, 1, row, 1, 1);
    
    row++;
    label = gtk_label_new("Framerate:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->framerate_spin = gtk_spin_button_new_with_range(1, 120, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->framerate_spin), 30);
    gtk_grid_attach(GTK_GRID(config_grid), app->framerate_spin, 1, row, 1, 1);

    row++;
    label = gtk_label_new("Rotate (0-3):");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->rotate_spin = gtk_spin_button_new_with_range(0, 3, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->rotate_spin), 0);
    gtk_grid_attach(GTK_GRID(config_grid), app->rotate_spin, 1, row, 1, 1);
    
    // Control buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 10);
    
    GtkWidget *test_btn = gtk_button_new_with_label("Test Connection");
    g_signal_connect(test_btn, "clicked", G_CALLBACK(on_test_connection_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), test_btn, FALSE, FALSE, 0);
    
    GtkWidget *connect_btn = gtk_button_new_with_label("Connect");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_connect_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), connect_btn, FALSE, FALSE, 0);
    
    GtkWidget *stream_btn = gtk_button_new_with_label("Open Stream");
    app->stream_button = stream_btn;
    g_signal_connect(stream_btn, "clicked", G_CALLBACK(on_open_stream_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), stream_btn, FALSE, FALSE, 0);
    
    // Status label
    app->status_label = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(main_box), app->status_label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(app->window);
}

void ui_update_status(App *app, const char *status) {
    if (app->status_label) {
        gtk_label_set_text(GTK_LABEL(app->status_label), status);
    }
    printf("Status: %s\n", status);
}
