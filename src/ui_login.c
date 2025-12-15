#include "f1sh_camera_rx.h"

struct {
    char *input_username;
    char *input_password;
} input;

/* Username & password (1 only) */
char *username = "admin";
char *password = "steam4vietnam";

static void on_login_button_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) {
        ui_log(app, "Error: app is NULL in configuration");
        return;
    }
    const gchar *user = gtk_entry_get_text(GTK_ENTRY(app->username_entry));
    const gchar *pass = gtk_entry_get_text(GTK_ENTRY(app->password_entry));
    if ((g_strcmp0(user, username) == 0) && (g_strcmp0(pass, password) == 0)) {
        gtk_widget_destroy(app->login_window);
        ui_log(app, "Opened advanced configuration settings");
        ui_configuration(app);
        return;
    }
    ui_log(app, "Wrong username/password");
}

void ui_login(App *app) {
    // Login window
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
    gtk_widget_set_margin_end(login_grid, 15);
    gtk_widget_set_margin_top(login_grid, 15);
    gtk_widget_set_margin_bottom(login_grid, 15);
    
    // Username
    int row = 0;
    GtkWidget *label = gtk_label_new("Username:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(login_grid), label, 0, row, 1, 1);
    app->username_entry = gtk_entry_new();
    if (input.input_username) gtk_entry_set_text(GTK_ENTRY(app->username_entry), input.input_username);
    else gtk_entry_set_text(GTK_ENTRY(app->username_entry), "");
    gtk_grid_attach(GTK_GRID(login_grid), app->username_entry, 1, row, 1, 1);

    // Password
    row++;
    label = gtk_label_new("Password:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(login_grid), label, 0, row, 1, 1);
    app->password_entry = gtk_entry_new();
    if (input.input_password) gtk_entry_set_text(GTK_ENTRY(app->password_entry), input.input_password);
    else gtk_entry_set_text(GTK_ENTRY(app->password_entry), "");
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
