#include "f1sh_camera_rx.h"

typedef struct {
    const char *label;
    int width;
    int height;
} ResolutionPreset;

struct {
    char *input_username;
    char *input_password;
} input;

// Username & password (1 only)
char *username = "admin";
char *password = "steam4vietnam";


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
    if (!app || !GTK_IS_COMBO_BOX(app->resolution_combo)) {
        return;
    }

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
    if (!app || !GTK_IS_COMBO_BOX(app->framerate_combo)) {
        return;
    }

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
    if (!app || !GTK_IS_SPIN_BUTTON(app->rotate_spin)) {
        return;
    }

    app->config.rotate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->rotate_spin));
    apply_resolution_selection(app);
}

static void on_test_connection_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in test connection\n");
        return;
    }
    
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

static void on_connect_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in connect\n");
        return;
    }
    
    if (app->config.tx_server_ip) {
        g_free(app->config.tx_server_ip);
        app->config.tx_server_ip = NULL;
    }
    app->config.tx_server_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->tx_ip_entry)));
    app->config.rotate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->rotate_spin));
    
    apply_resolution_selection(app);
    apply_framerate_selection(app);
    if (app->config.rx_host_ip) {
        g_free(app->config.rx_host_ip);
        app->config.rx_host_ip = NULL;
    }
    app->config.rx_host_ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(app->rx_ip_entry)));
    
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

static void on_widget_destroy(GtkWidget *widget __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in widget destroy\n");
        return;
    }

    printf("Closed Widget\n");
    gtk_widget_destroy(widget);
}

static void on_config_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in configuration\n");
        return;
    }

    ui_login(app);
    printf("Opened login window\n");
}

static void on_login_button_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    
    if (!app) {
        printf("Error: app is NULL in configuration\n");
        return;
    }

    const gchar *user = gtk_entry_get_text(GTK_ENTRY(app->username_entry));
    const gchar *pass = gtk_entry_get_text(GTK_ENTRY(app->password_entry));
    if ((g_strcmp0(user, username) == 0) && (g_strcmp0(pass, password) == 0)) {
        gtk_widget_destroy(app->login_window);
        printf("Opened advanced configuration settings\n");
        ui_configuration(app);
        return;
    }

    printf("Wrong username/password\n");
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

    // Wifi box
    GtkWidget *wifi_frame = gtk_frame_new("Available Wifi(s)");
    gtk_box_pack_start(GTK_BOX(main_box), wifi_frame, FALSE, FALSE, 0);
    
    GtkWidget *wifi_grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(wifi_frame), wifi_grid);
    gtk_grid_set_row_spacing(GTK_GRID(wifi_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(wifi_grid), 10);
    gtk_widget_set_margin_start(wifi_grid, 15);
    gtk_widget_set_margin_end(wifi_grid, 15);
    gtk_widget_set_margin_top(wifi_grid, 15);
    gtk_widget_set_margin_bottom(wifi_grid, 15);

    // Camera connection status
    if (!http_test_connection(app)) {
        app->camera_status = gtk_label_new("No camera connected");
        gtk_box_pack_start(GTK_BOX(main_box), app->camera_status, FALSE, FALSE, 0);
    } else {
        app->camera_status = gtk_label_new("Connected to camera");
        gtk_box_pack_start(GTK_BOX(main_box), app->camera_status, FALSE, FALSE, 0);
    }

    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 10);

    // Connect button
    GtkWidget *connect_btn = gtk_button_new_with_label("Connect");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_connect_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), connect_btn, FALSE, FALSE, 0);

    // Stream button
    GtkWidget *stream_btn = gtk_button_new_with_label("Open Stream");
    app->stream_button = stream_btn;
    g_signal_connect(stream_btn, "clicked", G_CALLBACK(on_open_stream_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), stream_btn, FALSE, FALSE, 0);

    // Configuration button
    GtkWidget *config_btn = gtk_button_new_with_label("Advanced Configuration");
    g_signal_connect(config_btn, "clicked", G_CALLBACK(on_config_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), config_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(app->window);
}

void ui_login(App *app) {
    // Main window
    app->login_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->login_window), "User Login");
    gtk_window_set_default_size(GTK_WINDOW(app->login_window), 400, 200);
    gtk_window_set_position(GTK_WINDOW(app->login_window), GTK_WIN_POS_CENTER);
    g_signal_connect(app->login_window, "destroy", G_CALLBACK(on_widget_destroy), app);

    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(app->login_window), main_box);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);

    // Login section
    GtkWidget *login_frame = gtk_frame_new("Please login!");
    gtk_box_pack_start(GTK_BOX(main_box), login_frame, FALSE, FALSE, 0);

    GtkWidget *login_grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(login_frame), login_grid);
    gtk_grid_set_row_spacing(GTK_GRID(login_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(login_grid), 10);
    gtk_widget_set_margin_start(login_grid, 15);
    gtk_widget_set_margin_end(login_grid, 15);
    gtk_widget_set_margin_top(login_grid, 15);
    gtk_widget_set_margin_bottom(login_grid, 15);
    
    // Username
    int row = 0;
    GtkWidget *label = gtk_label_new("Username:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(login_grid), label, 0, row, 1, 1);
    app->username_entry = gtk_entry_new();
    if (input.input_username) {
        gtk_entry_set_text(GTK_ENTRY(app->username_entry), input.input_username);
    } else {
        gtk_entry_set_text(GTK_ENTRY(app->username_entry), "");
    }
    gtk_grid_attach(GTK_GRID(login_grid), app->username_entry, 1, row, 1, 1);

    // Password
    row++;
    label = gtk_label_new("Password:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(login_grid), label, 0, row, 1, 1);
    app->password_entry = gtk_entry_new();
    if (input.input_password) {
        gtk_entry_set_text(GTK_ENTRY(app->password_entry), input.input_password);
    } else {
        gtk_entry_set_text(GTK_ENTRY(app->password_entry), "");
    }   
    gtk_entry_set_visibility(GTK_ENTRY(app->password_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(app->password_entry), '*');
    gtk_grid_attach(GTK_GRID(login_grid), app->password_entry, 1, row, 1, 1);

    // Login button
    GtkWidget *login_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(login_button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), login_button_box, FALSE, FALSE, 10);

    GtkWidget *login_btn = gtk_button_new_with_label("Login");
    g_signal_connect(login_btn, "clicked", G_CALLBACK(on_login_button_clicked), app);
    gtk_box_pack_start(GTK_BOX(login_button_box), login_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(app->login_window);
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
    if (app->config.tx_server_ip) {
        gtk_entry_set_text(GTK_ENTRY(app->tx_ip_entry), app->config.tx_server_ip);
    } else {
        gtk_entry_set_text(GTK_ENTRY(app->tx_ip_entry), "127.0.0.1");
    }
    gtk_grid_attach(GTK_GRID(config_grid), app->tx_ip_entry, 1, row, 1, 1);

    row++;
    label = gtk_label_new("RX Host IP:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(config_grid), label, 0, row, 1, 1);
    app->rx_ip_entry = gtk_entry_new();
    if (app->config.rx_host_ip) {
        gtk_entry_set_text(GTK_ENTRY(app->rx_ip_entry), app->config.rx_host_ip);
    } else {
        gtk_entry_set_text(GTK_ENTRY(app->rx_ip_entry), "127.0.0.1");
    }
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
        if (app->config.width == kResolutionPresets[i].width &&
            app->config.height == kResolutionPresets[i].height) {
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

    
    // Control buttons
    GtkWidget *config_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(config_button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), config_button_box, FALSE, FALSE, 10);
    
    GtkWidget *test_btn = gtk_button_new_with_label("Test Connection");
    g_signal_connect(test_btn, "clicked", G_CALLBACK(on_test_connection_clicked), app);
    gtk_box_pack_start(GTK_BOX(config_button_box), test_btn, FALSE, FALSE, 0);
    
    GtkWidget *connect_btn = gtk_button_new_with_label("Connect");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_connect_clicked), app);
    gtk_box_pack_start(GTK_BOX(config_button_box), connect_btn, FALSE, FALSE, 0);
    
    GtkWidget *stream_btn = gtk_button_new_with_label("Open Stream");
    app->stream_button = stream_btn;
    g_signal_connect(stream_btn, "clicked", G_CALLBACK(on_open_stream_clicked), app);
    gtk_box_pack_start(GTK_BOX(config_button_box), stream_btn, FALSE, FALSE, 0);
    
    // Status label
    app->config_status_label = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(main_box), app->config_status_label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(app->config_window);
}

void ui_update_status(App *app, const char *status) {
    if (app->config_status_label) {
        gtk_label_set_text(GTK_LABEL(app->config_status_label), status);
    }
    printf("Status: %s\n", status);
}
