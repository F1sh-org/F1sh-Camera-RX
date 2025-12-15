#include "f1sh_camera_rx.h"

typedef struct {
    const char *label;
    int width;
    int height;
} ResolutionPreset;

static const ResolutionPreset kResolutionPresets[] = {
    { "1280 x 720", 1280, 720 },
    { "1920 x 1080", 1920, 1080 },
};

static const int kFrameratePresets[] = { 30, 50, 60 };

static gboolean rotate_is_horizontal(int rotate)
{
    return (rotate % 2) != 0; // rotate = 1 or 3
}

static void apply_resolution_selection(App *app) {
    if (!app || !GTK_IS_COMBO_BOX(app->resolution_combo)) return;
    gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(app->resolution_combo));
    if (index < 0 || index >= (gint)G_N_ELEMENTS(kResolutionPresets)) {
        index = 0;
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->resolution_combo), index);
    }
    app->config.width = kResolutionPresets[index].width;
    app->config.height = kResolutionPresets[index].height;
    if (rotate_is_horizontal(app->config.rotate)) {
        gint tmp = app->config.width;
        app->config.width = app->config.height;
        app->config.height = tmp;
    }
}

static void apply_framerate_selection(App *app) {
    if (!app || !GTK_IS_COMBO_BOX(app->framerate_combo)) return;
    gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(app->framerate_combo));
    if (index < 0 || index >= (gint)G_N_ELEMENTS(kFrameratePresets)) {
        index = 0;
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->framerate_combo), index);
    }
    app->config.framerate = kFrameratePresets[index];
}

static void on_resolution_changed(GtkComboBox *combo __attribute__((unused)), gpointer user_data) {
    apply_resolution_selection((App*)user_data);
}

static void on_framerate_changed(GtkComboBox *combo __attribute__((unused)), gpointer user_data) {
    apply_framerate_selection((App*)user_data);
}

static void on_rotate_changed(GtkSpinButton *spin __attribute__((unused)), gpointer user_data)
{
    App *app = (App*)user_data;
    if (!app || !GTK_IS_SPIN_BUTTON(app->rotate_spin)) return;
    app->config.rotate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->rotate_spin));
    apply_resolution_selection(app);
}

static void on_test_connection_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) return;
    if (app->config.tx_server_ip) {
        g_free(app->config.tx_server_ip);
        app->config.tx_server_ip = NULL;
    }
    app->config.tx_server_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->tx_ip_entry)));
    app->config.rotate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->rotate_spin));
    apply_resolution_selection(app);
    apply_framerate_selection(app);
    if (http_test_connection(app)) {
        ui_update_status(app, "Connection OK");
    } else {
        ui_update_status(app, "Connection Failed");
    }
}

static void on_save_config_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) return;
    if (app->config.tx_server_ip) {
        g_free(app->config.tx_server_ip);
        app->config.tx_server_ip = NULL;
    }
    app->config.tx_server_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->tx_ip_entry)));
    app->config.rotate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->rotate_spin));
    apply_resolution_selection(app);
    apply_framerate_selection(app);
    if (app->config.rx_host_ip) { g_free(app->config.rx_host_ip); app->config.rx_host_ip = NULL; }
    app->config.rx_host_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->rx_ip_entry)));
    // Use port 8888 for RX stream in saved configuration
    app->config.rx_stream_port = 8888;
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
    ui_update_status(app, "Configuration saved to TX server");
}

void on_open_stream_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) {
        ui_log(app, "Error: app is NULL in open stream");
        return;
    }
    if (app->connected) {
        // Stop streaming
        stream_stop(app);
        app->connected = FALSE;
        if (app->stream_button_main) gtk_button_set_label(GTK_BUTTON(app->stream_button_main), "Open Stream");
        // Stream controls removed from advanced configuration
        ui_update_status(app, "Stream stopped");
        return;
    }
    
    // Coordinate rotation with TX: 0/180 => /noswap, 90/270 => /swap
    if (app->config.rotate == 0 || app->config.rotate == 2) {
        if (!http_request_noswap(app)) {
            ui_log(app, "Unable to configure camera's rotation (noswap)");
        }
    } else if (app->config.rotate == 1 || app->config.rotate == 3) {
        if (!http_request_swap(app)) {
            ui_log(app, "Unable to configure camera's rotation (swap)");
        }
    }

    // Start streaming (assumes config was already sent)
    if (stream_start(app)) {
        app->connected = TRUE;
        if (app->stream_button_main) gtk_button_set_label(GTK_BUTTON(app->stream_button_main), "Stop Stream");
        ui_update_status(app, "Streaming");
    } else {
        ui_update_status(app, "Failed to start stream");
    }
}

static void on_log_interactive_toggled(GtkSwitch *sw, GParamSpec *pspec, gpointer user_data) {
    (void)pspec; // unused
    App *app = (App*)user_data;
    if (!app) return;
    gboolean active = gtk_switch_get_active(GTK_SWITCH(sw));
    app->log_interactive = active;
    ui_set_log_interactive(app, active);
}

// Update visible widgets to reflect the current app->config values
static void update_config_widgets(App *app) {
    if (!app) return;
    if (app->rx_ip_entry && app->config.rx_host_ip) {
        gtk_entry_set_text(GTK_ENTRY(app->rx_ip_entry), app->config.rx_host_ip);
    }
    // Resolution combo
    if (app->resolution_combo) {
        gint resolution_index = 0;
        for (guint i = 0; i < G_N_ELEMENTS(kResolutionPresets); ++i) {
            if (app->config.width == kResolutionPresets[i].width && app->config.height == kResolutionPresets[i].height) {
                resolution_index = (gint)i;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->resolution_combo), resolution_index);
        apply_resolution_selection(app);
    }
    // Framerate combo
    if (app->framerate_combo) {
        gint framerate_index = 0;
        for (guint i = 0; i < G_N_ELEMENTS(kFrameratePresets); ++i) {
            if (app->config.framerate == kFrameratePresets[i]) {
                framerate_index = (gint)i;
                break;
            }
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->framerate_combo), framerate_index);
        apply_framerate_selection(app);
    }
    // Rotate spin
    if (app->rotate_spin) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->rotate_spin), app->config.rotate);
    }
}

// If a COM port is connected, fetch TX config over serial (status 5) and update local config/UI
static void refresh_tx_config_from_serial(App *app) {
    if (!app || !app->serial_status_label) return;
    const char *txt = gtk_label_get_text(GTK_LABEL(app->serial_status_label));
    if (!txt || !strstr(txt, "COM port:")) return;

    // Extract port name from label
    const char *p = strchr(txt, ':');
    char portbuf[256] = {0};
    if (p) {
        while (*p == ':' || *p == ' ') p++;
        strncpy(portbuf, p, sizeof(portbuf) - 1);
    }
    if (!portbuf[0]) return;

    const char *cfg_cmd = "{\"status\":5}\n";
    ui_log(app, "Requesting TX config via serial %s (advanced config open)", portbuf);
    char *cfg_resp = serial_send_receive(app, portbuf, cfg_cmd, 4000);
    if (!cfg_resp) {
        ui_log(app, "No serial response when requesting TX config in advanced config window");
        return;
    }

    ui_log(app, "TX config serial response: %s", cfg_resp);
    json_error_t cerr;
    json_t *cfg_root = json_loads(cfg_resp, 0, &cerr);
    g_free(cfg_resp);
    if (!cfg_root) {
        ui_log(app, "Failed to parse TX config JSON from serial: %s", cerr.text);
        return;
    }

    json_t *cfg_status = json_object_get(cfg_root, "status");
    json_t *cfg_payload = json_object_get(cfg_root, "payload");
    json_t *cfg_obj = NULL;
    if (json_is_object(cfg_payload)) {
        cfg_obj = cfg_payload;
        json_incref(cfg_obj);
    } else if (json_is_string(cfg_payload)) {
        const char *payload_str = json_string_value(cfg_payload);
        if (payload_str) {
            json_error_t perr;
            json_t *parsed = json_loads(payload_str, 0, &perr);
            if (parsed && json_is_object(parsed)) {
                cfg_obj = parsed; // owns ref
            } else if (parsed) {
                json_decref(parsed);
            }
        }
    }

    if (json_is_integer(cfg_status) && json_integer_value(cfg_status) == 5 && cfg_obj) {
        json_t *host = json_object_get(cfg_obj, "host");
        json_t *port = json_object_get(cfg_obj, "port");
        json_t *width = json_object_get(cfg_obj, "width");
        json_t *height = json_object_get(cfg_obj, "height");
        json_t *fr = json_object_get(cfg_obj, "framerate");

        if (json_is_string(host)) {
            const char *h = json_string_value(host);
            if (h) {
                if (app->config.rx_host_ip) g_free(app->config.rx_host_ip);
                app->config.rx_host_ip = g_strdup(h);
            }
        }
        if (json_is_integer(port)) app->config.rx_stream_port = (int)json_integer_value(port);
        if (json_is_integer(width)) app->config.width = (int)json_integer_value(width);
        if (json_is_integer(height)) app->config.height = (int)json_integer_value(height);
        if (json_is_integer(fr)) app->config.framerate = (int)json_integer_value(fr);

        update_config_widgets(app);
    }

    if (cfg_obj) json_decref(cfg_obj);
    json_decref(cfg_root);
}

void ui_configuration(App *app) {
    // New window
    app->config_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->config_window), "Advanced Configuration");
    gtk_window_set_default_size(GTK_WINDOW(app->config_window), 600, 400);
    gtk_window_set_position(GTK_WINDOW(app->config_window), GTK_WIN_POS_CENTER);
    g_signal_connect(app->config_window, "destroy", G_CALLBACK(on_widget_destroy), app);
    
    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(app->config_window), main_box);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    
    // Configuration section
    GtkWidget *config_frame = gtk_frame_new("Advanced Configuration");
    gtk_box_pack_start(GTK_BOX(main_box), config_frame, FALSE, FALSE, 0);
    GtkWidget *config_grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(config_frame), config_grid);
    gtk_grid_set_row_spacing(GTK_GRID(config_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(config_grid), 10);
    gtk_widget_set_margin_start(config_grid, 15);
    gtk_widget_set_margin_end(config_grid, 15);
    gtk_widget_set_margin_top(config_grid, 15);
    gtk_widget_set_margin_bottom(config_grid, 15);
    
    int row = 0;
    GtkWidget *label = gtk_label_new("TX Server IP:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->tx_ip_entry = gtk_entry_new();
    if (app->config.tx_server_ip) gtk_entry_set_text(GTK_ENTRY(app->tx_ip_entry), app->config.tx_server_ip);
    else gtk_entry_set_text(GTK_ENTRY(app->tx_ip_entry), "127.0.0.1");
    gtk_grid_attach(GTK_GRID(config_grid), app->tx_ip_entry, 1, row, 1, 1);

    row++;
    label = gtk_label_new("RX Server IP:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->rx_ip_entry = gtk_entry_new();
    if (app->config.rx_host_ip) gtk_entry_set_text(GTK_ENTRY(app->rx_ip_entry), app->config.rx_host_ip);
    else gtk_entry_set_text(GTK_ENTRY(app->rx_ip_entry), "127.0.0.1");
    gtk_grid_attach(GTK_GRID(config_grid), app->rx_ip_entry, 1, row, 1, 1);

    row++;
    label = gtk_label_new("Resolution Preset:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->resolution_combo = gtk_combo_box_text_new();
    for (guint i = 0; i < G_N_ELEMENTS(kResolutionPresets); ++i) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->resolution_combo), kResolutionPresets[i].label);
    }
    gint resolution_index = 0;
    for (guint i = 0; i < G_N_ELEMENTS(kResolutionPresets); ++i) {
        if (app->config.width == kResolutionPresets[i].width && app->config.height == kResolutionPresets[i].height) {
            resolution_index = (gint)i;
            break;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->resolution_combo), resolution_index);
    g_signal_connect(app->resolution_combo, "changed", G_CALLBACK(on_resolution_changed), app);
    gtk_grid_attach(GTK_GRID(config_grid), app->resolution_combo, 1, row, 1, 1);
    apply_resolution_selection(app);

    row++;
    label = gtk_label_new("Framerate Preset:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->framerate_combo = gtk_combo_box_text_new();
    for (guint i = 0; i < G_N_ELEMENTS(kFrameratePresets); ++i) {
        char text[32];
        snprintf(text, sizeof(text), "%d fps", kFrameratePresets[i]);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->framerate_combo), text);
    }
    gint framerate_index = 0;
    for (guint i = 0; i < G_N_ELEMENTS(kFrameratePresets); ++i) {
        if (app->config.framerate == kFrameratePresets[i]) {
            framerate_index = (gint)i;
            break;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->framerate_combo), framerate_index);
    g_signal_connect(app->framerate_combo, "changed", G_CALLBACK(on_framerate_changed), app);
    gtk_grid_attach(GTK_GRID(config_grid), app->framerate_combo, 1, row, 1, 1);
    apply_framerate_selection(app);

    row++;
    label = gtk_label_new("Rotate (0-3):");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->rotate_spin = gtk_spin_button_new_with_range(0, 3, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->rotate_spin), app->config.rotate);
    gtk_grid_attach(GTK_GRID(config_grid), app->rotate_spin, 1, row, 1, 1);
    g_signal_connect(app->rotate_spin, "value-changed", G_CALLBACK(on_rotate_changed), app);

    // Log interactivity toggle
    row++;
    label = gtk_label_new("Allow Log Interaction:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->log_interactive_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(app->log_interactive_switch), app->log_interactive);
    g_signal_connect(app->log_interactive_switch, "notify::active", G_CALLBACK(on_log_interactive_toggled), app);
    gtk_grid_attach(GTK_GRID(config_grid), app->log_interactive_switch, 1, row, 1, 1);


    // Control buttons
    GtkWidget *config_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(config_button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), config_button_box, FALSE, FALSE, 10);
    
    GtkWidget *test_btn = gtk_button_new_with_label("Test Connection");
    g_signal_connect(test_btn, "clicked", G_CALLBACK(on_test_connection_clicked), app);
    gtk_box_pack_start(GTK_BOX(config_button_box), test_btn, FALSE, FALSE, 0);
    
    GtkWidget *connect_btn = gtk_button_new_with_label("Save config");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_save_config_clicked), app);
    gtk_box_pack_start(GTK_BOX(config_button_box), connect_btn, FALSE, FALSE, 0);
    
    // Status label
    app->config_status_label = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(main_box), app->config_status_label, FALSE, FALSE, 0);

    // If a camera is still connected over serial, pull TX config now
    refresh_tx_config_from_serial(app);

    gtk_widget_show_all(app->config_window);
}
