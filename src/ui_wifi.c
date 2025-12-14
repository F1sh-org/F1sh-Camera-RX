#include "f1sh_camera_rx.h"

typedef struct {
    App *app;
    char *ssid;
    char *bssid;
    int signal;
} WifiEntry;

static void free_entries_on_destroy(GtkWidget *w, gpointer user_data) {
    (void)w;
    GPtrArray *a = (GPtrArray*)user_data;
    if (!a) return;
    // Clear app->wifi_window if present
    if (a->len > 0) {
        WifiEntry *we0 = g_ptr_array_index(a, 0);
        if (we0 && we0->app) we0->app->wifi_window = NULL;
    }
    for (guint i = 0; i < a->len; ++i) {
        WifiEntry *we = g_ptr_array_index(a, i);
        if (we) {
            if (we->ssid) g_free(we->ssid);
            if (we->bssid) g_free(we->bssid);
            g_free(we);
        }
    }
    g_ptr_array_free(a, TRUE);
}

static void on_password_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    (void)dialog;
    typedef struct { App *app; char *ssid; char *bssid; GtkWidget *dialog; } PasswordDialogData;
    PasswordDialogData *d = user_data;
    if (!d) return;
    if (response_id == GTK_RESPONSE_OK) {
        GtkWidget *entry = g_object_get_data(G_OBJECT(d->dialog), "password_entry");
        const char *pass = gtk_entry_get_text(GTK_ENTRY(entry));
        if (d->app) {
            ui_log(d->app, "Sending password for SSID %s", d->ssid ? d->ssid : "(unknown)");
            ui_send_wifi_credentials(d->app, d->bssid, pass);
        }
    }
    if (GTK_IS_WINDOW(d->dialog)) gtk_widget_destroy(d->dialog);
    if (d->ssid) g_free(d->ssid);
    if (d->bssid) g_free(d->bssid);
    g_free(d);
}

static void on_wifi_selected(GtkButton *button, gpointer user_data) {
    WifiEntry *e = (WifiEntry*)user_data;
    if (!e) return;

    App *app = e->app;
    ui_log(app, "Selected Wi-Fi: %s (signal %d)", e->ssid ? e->ssid : "(unknown)", e->signal);

    // Create password dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Enter Wi-Fi Password",
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
                                                    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Connect", GTK_RESPONSE_OK,
                                                    NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    gtk_box_pack_start(GTK_BOX(content), box, TRUE, TRUE, 0);

    GtkWidget *label = gtk_label_new("Enter password for the selected WiFi network:");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(entry), '*');
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);

    // Store entry pointer for response handler
    g_object_set_data(G_OBJECT(dialog), "password_entry", entry);

    typedef struct { App *app; char *ssid; char *bssid; GtkWidget *dialog; } PasswordDialogData;
    PasswordDialogData *d = g_new0(PasswordDialogData, 1);
    d->app = app;
    d->ssid = e->ssid ? g_strdup(e->ssid) : NULL;
    d->bssid = e->bssid ? g_strdup(e->bssid) : NULL;
    d->dialog = dialog;

    g_signal_connect(dialog, "response", G_CALLBACK(on_password_dialog_response), d);
    gtk_widget_show_all(dialog);
}

static int compare_entries(const void *pa, const void *pb) {
    const WifiEntry *a = *(const WifiEntry**)pa;
    const WifiEntry *b = *(const WifiEntry**)pb;
    if (!a || !b) return 0;
    // descending order (higher signal first)
    return (b->signal - a->signal);
}

void ui_wifi_show_list(App *app, json_t *list) {
    if (!app || !json_is_array(list)) return;

    size_t i, n = json_array_size(list);
    if (n == 0) {
        ui_log(app, "No Wi-Fi networks found");
        return;
    }

    GPtrArray *arr = g_ptr_array_new_with_free_func(g_free);
    for (i = 0; i < n; ++i) {
        json_t *obj = json_array_get(list, i);
        if (!json_is_object(obj)) continue;
        const char *ssid = json_string_value(json_object_get(obj, "SSID"));
        const char *bssid = json_string_value(json_object_get(obj, "BSSID"));
        json_t *sig = json_object_get(obj, "signal_dbm");
        int s = 0;
        if (json_is_integer(sig)) s = (int)json_integer_value(sig);
        else if (json_is_real(sig)) s = (int)json_real_value(sig);

        WifiEntry *we = g_new0(WifiEntry, 1);
        we->app = app;
        we->ssid = ssid ? g_strdup(ssid) : g_strdup("(unnamed)");
        we->bssid = bssid ? g_strdup(bssid) : NULL;
        we->signal = s;
        g_ptr_array_add(arr, we);
    }

    // Sort by signal descending (higher is better; e.g., -40 > -80)
    g_ptr_array_sort(arr, compare_entries);

    // Create window
    app->wifi_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->wifi_window), "Select Wi-Fi Network");
    gtk_window_set_default_size(GTK_WINDOW(app->wifi_window), 400, 300);
    gtk_window_set_transient_for(GTK_WINDOW(app->wifi_window), GTK_WINDOW(app->window));

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    gtk_container_add(GTK_CONTAINER(app->wifi_window), box);
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(sc, TRUE);
    gtk_container_add(GTK_CONTAINER(box), sc);

    GtkWidget *listbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(sc), listbox);

    // Query local system for the currently connected SSID (Windows only)
    WifiInfo current = get_wifi_info();
    NetworkInfo net = get_network_info();

    for (i = 0; i < arr->len; ++i) {
        WifiEntry *we = g_ptr_array_index(arr, i);
        char label[512];
        if (current.success && strcmp(current.ssid, we->ssid) == 0) {
            snprintf(label, sizeof(label), "%s (currently connecting) (%d dBm)", we->ssid, we->signal);
        } else {
            snprintf(label, sizeof(label), "%s (%d dBm)", we->ssid, we->signal);
        }
        GtkWidget *btn = gtk_button_new_with_label(label);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_wifi_selected), we);
        gtk_box_pack_start(GTK_BOX(listbox), btn, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(app->wifi_window);

    // If we have a valid local IP, send it to the TX server as status 23
    if (net.success && net.ip[0] != '\0' && app->config.tx_server_ip) {
        // Best-effort, log failures but do not block UI
        ui_log(app, "Reporting local IP %s to TX", net.ip);
        if (!http_send_ip_addr(app, net.ip)) {
            ui_log(app, "Failed to send local IP to TX server: %s", net.ip);
        }
    }

    // Free entries when window is destroyed
    g_signal_connect(app->wifi_window, "destroy", G_CALLBACK(free_entries_on_destroy), arr);
}

