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

// Send a command to given serial port and read response (blocking). Returns a newly allocated string or NULL.
static char *serial_send_receive(App *app, const char *port, const char *cmd, int timeout_ms)
{
    if (!app || !port || !cmd) return NULL;
    char readbuf[4096];

#if defined(_WIN32) || defined(__CYGWIN__)
    char path[64];
    if (strncmp(port, "COM", 3) == 0) {
        int n = atoi(port + 3);
        if (n >= 10) snprintf(path, sizeof(path), "\\\\.\\%s", port);
        else snprintf(path, sizeof(path), "%s", port);
    } else {
        snprintf(path, sizeof(path), "%s", port);
    }

    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); return NULL; }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(h, &dcb);

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = timeout_ms;
    timeouts.WriteTotalTimeoutConstant = 200;
    SetCommTimeouts(h, &timeouts);

    DWORD written = 0;
    WriteFile(h, cmd, (DWORD)strlen(cmd), &written, NULL);

    DWORD read = 0;
    BOOL ok = ReadFile(h, readbuf, (DWORD)(sizeof(readbuf) - 1), &read, NULL);
    if (read > 0 && read < sizeof(readbuf)) readbuf[read] = '\0'; else readbuf[0] = '\0';
    CloseHandle(h);
    if (!ok) return NULL;
    return g_strdup(readbuf);

#else // POSIX
    int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return NULL;

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) { close(fd); return NULL; }
    cfmakeraw(&tio);
    cfsetspeed(&tio, B115200);
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = (timeout_ms + 99) / 100; // deciseconds
    if (tcsetattr(fd, TCSANOW, &tio) != 0) { close(fd); return NULL; }

    tcflush(fd, TCIOFLUSH);
    ssize_t w = write(fd, cmd, strlen(cmd)); (void)w;

    ssize_t r = read(fd, readbuf, sizeof(readbuf) - 1);
    if (r > 0 && r < (ssize_t)sizeof(readbuf)) readbuf[r] = '\0'; else readbuf[0] = '\0';

    close(fd);
    if (r <= 0) return NULL;
    return g_strdup(readbuf);
#endif
}

// Worker data for setup command
typedef struct { App *app; char *port; } SetupWorker;

static gboolean setup_complete_idle(gpointer data) {
    SetupWorker *w = (SetupWorker*)data;
    if (!w) return FALSE;
    if (w->port) g_free(w->port);
    // Re-enable setup button if serial still present
    if (w->app && w->app->setup_button) gtk_widget_set_sensitive(w->app->setup_button, TRUE);
    g_free(w);
    return FALSE;
}

// Idle function to send wifi list to UI
static gboolean wifi_list_idle(gpointer data) {
    struct { App *app; json_t *list; } *p = data;
    if (p && p->app && p->list) {
        ui_wifi_show_list(p->app, p->list);
        json_decref(p->list);
    }
    g_free(p);
    return FALSE;
}

// Background thread: send status 21 and parse response
static gpointer setup_camera_thread(gpointer user_data) {
    SetupWorker *w = (SetupWorker*)user_data;
    App *app = w->app;
    const char *cmd = "{\"status\":21}\n";
    char *resp = serial_send_receive(app, w->port, cmd, 3000);
    if (!resp) {
        ui_log(app, "No response to setup command on %s", w->port ? w->port : "(null)");
        g_idle_add(setup_complete_idle, w);
        return NULL;
    }
    ui_log(app, "Setup response: %s", resp);

    // Parse JSON
    json_error_t err;
    json_t *root = json_loads(resp, 0, &err);
    g_free(resp);
    if (!root) {
        ui_log(app, "Invalid JSON from camera: %s", err.text);
        g_idle_add(setup_complete_idle, w);
        return NULL;
    }

    json_t *status = json_object_get(root, "status");
    json_t *payload = json_object_get(root, "payload");
    if (!json_is_integer(status) || json_integer_value(status) != 4 || !json_is_array(payload)) {
        ui_log(app, "Unexpected setup reply from camera");
        json_decref(root);
        g_idle_add(setup_complete_idle, w);
        return NULL;
    }

    // Increase ref for passing into idle/UI thread
    json_incref(payload);
    typedef struct { App *app; json_t *list; } WifiListIdle;
    WifiListIdle *pl = g_new0(WifiListIdle, 1);
    pl->app = app;
    pl->list = payload;
    g_idle_add(wifi_list_idle, pl);

    json_decref(root);
    g_idle_add(setup_complete_idle, w);
    return NULL;
}

void on_setup_camera_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) return;
    // Check label to ensure serial camera present
    const char *txt = gtk_label_get_text(GTK_LABEL(app->serial_status_label));
    if (!txt || strstr(txt, "COM port:") == NULL) {
        ui_log(app, "No camera connected via serial, setup disabled");
        if (app->setup_button) gtk_widget_set_sensitive(app->setup_button, FALSE);
        return;
    }

    // Extract port from label text
    const char *p = strchr(txt, ':');
    char portbuf[256];
    if (p) {
        while (*p == ':' || *p == ' ') p++;
        strncpy(portbuf, p, sizeof(portbuf)-1);
        portbuf[sizeof(portbuf)-1] = '\0';
    } else {
        strncpy(portbuf, txt, sizeof(portbuf)-1);
        portbuf[sizeof(portbuf)-1] = '\0';
    }

    // Disable button while working
    if (app->setup_button) gtk_widget_set_sensitive(app->setup_button, FALSE);

    SetupWorker *w = g_new0(SetupWorker, 1);
    w->app = app;
    w->port = g_strdup(portbuf);
    g_thread_new("setup-camera", setup_camera_thread, w);
}

// --- Wi-Fi credential sender ---
typedef struct { App *app; char *bssid; char *pass; char *port; } WifiCredWorker;

static gboolean show_msg_idle(gpointer data) {
    struct { App *app; char *msg; gboolean is_error; } *d = data;
    if (!d) return FALSE;
    GtkWidget *parent = d->app ? d->app->window : NULL;
    GtkMessageType type = d->is_error ? GTK_MESSAGE_ERROR : GTK_MESSAGE_INFO;
    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_MODAL, type, GTK_BUTTONS_OK, "%s", d->msg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    g_free(d->msg);
    g_free(d);
    return FALSE;
}

static gboolean store_ip_idle(gpointer data) {
    struct { App *app; char *ip; } *d = data;
    if (!d) return FALSE;
    App *app = d->app;
    if (app) {
        if (app->config.tx_server_ip) g_free(app->config.tx_server_ip);
        app->config.tx_server_ip = g_strdup(d->ip);
        ui_log(app, "Stored camera IP: %s", d->ip);

        // Check for existing stored camera/tx config
        const char *store_path = "camera_tx.json";
        gboolean prompt_setup = FALSE;
        FILE *r = fopen(store_path, "r");
        if (r) {
            fseek(r, 0, SEEK_END);
            long sz = ftell(r);
            fseek(r, 0, SEEK_SET);
            if (sz > 0) {
                char *buf = g_malloc(sz + 1);
                size_t n = fread(buf, 1, sz, r);
                buf[n] = '\0';
                json_error_t err;
                json_t *root = json_loads(buf, 0, &err);
                g_free(buf);
                if (root) {
                    json_t *cip = json_object_get(root, "camera_ip");
                    if (!cip || !json_is_string(cip) || strcmp(json_string_value(cip), d->ip) != 0) {
                        prompt_setup = TRUE;
                    }
                    json_decref(root);
                } else {
                    prompt_setup = TRUE;
                }
            }
            fclose(r);
        } else {
            prompt_setup = TRUE;
        }

        if (prompt_setup) {
            ui_log(app, "No stored camera config found or IP changed; please run setup for this camera.");
        }

        // Try to query TX for its current configuration (status 5)
        json_t *txconf = NULL;
        if (http_request_tx_config(app, &txconf)) {
            // Build file JSON: { camera_ip: "...", tx: <txconf> }
            json_t *file_root = json_object();
            json_object_set_new(file_root, "camera_ip", json_string(d->ip));
            // txconf ownership transferred; insert directly
            json_object_set_new(file_root, "tx", txconf);

            char *out = json_dumps(file_root, JSON_INDENT(2));
            json_decref(file_root);
            if (out) {
                FILE *f = fopen(store_path, "w");
                if (f) {
                    fprintf(f, "%s\n", out);
                    fclose(f);
                }
                g_free(out);
            }
            ui_log(app, "Stored camera IP and TX config to %s", store_path);
        } else {
            // Fallback: write minimal info with only camera_ip
            json_t *file_root = json_object();
            json_object_set_new(file_root, "camera_ip", json_string(d->ip));
            json_object_set_new(file_root, "tx", json_object());
            char *out = json_dumps(file_root, JSON_INDENT(2));
            json_decref(file_root);
            if (out) {
                FILE *f = fopen(store_path, "w");
                if (f) {
                    fprintf(f, "%s\n", out);
                    fclose(f);
                }
                g_free(out);
            }
            ui_log(app, "Stored camera IP to %s (TX config query failed)", store_path);
        }

        // Update UI: enable stream buttons now that a camera has connected wirelessly
        if (app->stream_button_main) gtk_widget_set_sensitive(app->stream_button_main, TRUE);
        if (app->stream_button_config) gtk_widget_set_sensitive(app->stream_button_config, TRUE);

        // Inform user in dialog
        typedef struct { App *app; char *msg; gboolean is_error; } MsgData;
        MsgData *m = g_new0(MsgData, 1);
        m->app = app;
        m->msg = g_strdup_printf("Connected. Camera IP stored: %s", d->ip);
        m->is_error = FALSE;
        g_idle_add(show_msg_idle, m);
    }
    g_free(d->ip);
    g_free(d);
    return FALSE;
}

static gpointer send_wifi_thread(gpointer user_data) {
    WifiCredWorker *w = user_data;
    if (!w || !w->app) return NULL;
    App *app = w->app;

    if (!w->port) {
        // No serial port
        typedef struct { App *app; char *msg; gboolean is_error; } MsgData;
        MsgData *m = g_new0(MsgData, 1);
        m->app = app;
        m->msg = g_strdup("No serial COM port available to send Wi‑Fi credentials");
        m->is_error = TRUE;
        g_idle_add(show_msg_idle, m);
        g_free(w->bssid);
        g_free(w->pass);
        g_free(w);
        return NULL;
    }

    // Build inner JSON payload
    json_t *inner = json_object();
    json_object_set_new(inner, "BSSID", json_string(w->bssid));
    json_object_set_new(inner, "pass", json_string(w->pass));
    char *inner_str = json_dumps(inner, 0);
    json_decref(inner);
    if (!inner_str) inner_str = g_strdup("{}");

    // Build outer JSON with status 22 and payload as string
    json_t *root = json_object();
    json_object_set_new(root, "status", json_integer(22));
    json_object_set_new(root, "payload", json_string(inner_str));
    char *cmd = json_dumps(root, 0);
    json_decref(root);
    g_free(inner_str);

    if (!cmd) {
        typedef struct { App *app; char *msg; gboolean is_error; } MsgData;
        MsgData *m = g_new0(MsgData, 1);
        m->app = app;
        m->msg = g_strdup("Failed to create JSON for WiFi credentials");
        m->is_error = TRUE;
        g_idle_add(show_msg_idle, m);
        g_free(w->bssid);
        g_free(w->pass);
        g_free(w->port);
        g_free(w);
        return NULL;
    }

    // Append newline as protocol
    char *cmd_nl = g_strdup_printf("%s\n", cmd);
    g_free(cmd);

    ui_log(app, "Sending WiFi credentials to %s: %s", w->port, cmd_nl);
    char *resp = serial_send_receive(app, w->port, cmd_nl, 8000);
    g_free(cmd_nl);

    if (!resp) {
        typedef struct { App *app; char *msg; gboolean is_error; } MsgData;
        MsgData *m = g_new0(MsgData, 1);
        m->app = app;
        m->msg = g_strdup("No response from camera when sending WiFi credentials");
        m->is_error = TRUE;
        g_idle_add(show_msg_idle, m);
        g_free(w->bssid);
        g_free(w->pass);
        g_free(w->port);
        g_free(w);
        return NULL;
    }

    ui_log(app, "WiFi send response: %s", resp);

    json_error_t err;
    json_t *r = json_loads(resp, 0, &err);
    g_free(resp);
    if (!r) {
        typedef struct { App *app; char *msg; gboolean is_error; } MsgData;
        MsgData *m = g_new0(MsgData, 1);
        m->app = app;
        m->msg = g_strdup_printf("Invalid JSON response: %s", err.text);
        m->is_error = TRUE;
        g_idle_add(show_msg_idle, m);
        g_free(w->bssid);
        g_free(w->pass);
        g_free(w->port);
        g_free(w);
        return NULL;
    }

    json_t *status = json_object_get(r, "status");
    if (json_is_integer(status) && json_integer_value(status) == 3) {
        typedef struct { App *app; char *msg; gboolean is_error; } MsgData;
        MsgData *m = g_new0(MsgData, 1);
        m->app = app;
        m->msg = g_strdup("Failed to connect the wifi, please try again");
        m->is_error = TRUE;
        g_idle_add(show_msg_idle, m);
        json_decref(r);
        g_free(w->bssid);
        g_free(w->pass);
        g_free(w->port);
        g_free(w);
        return NULL;
    }

    if (json_is_integer(status) && json_integer_value(status) == 2) {
        json_t *ip = json_object_get(r, "IPAddr");
        if (json_is_string(ip)) {
            const char *ipstr = json_string_value(ip);
            typedef struct { App *app; char *ip; } IPData;
            IPData *d = g_new0(IPData, 1);
            d->app = app;
            d->ip = g_strdup(ipstr);
            g_idle_add(store_ip_idle, d);
        }
    }

    json_decref(r);
    g_free(w->bssid);
    g_free(w->pass);
    g_free(w->port);
    g_free(w);
    return NULL;
}

void ui_send_wifi_credentials(App *app, const char *bssid, const char *pass) {
    if (!app || !bssid) return;

    // Get current serial port from status label
    const char *txt = gtk_label_get_text(GTK_LABEL(app->serial_status_label));
    char portbuf[256] = {0};
    if (txt && strstr(txt, "COM port:")) {
        const char *p = strchr(txt, ':');
        if (p) {
            while (*p == ':' || *p == ' ') p++;
            strncpy(portbuf, p, sizeof(portbuf)-1);
        }
    }

    WifiCredWorker *w = g_new0(WifiCredWorker, 1);
    w->app = app;
    w->bssid = g_strdup(bssid);
    w->pass = pass ? g_strdup(pass) : g_strdup("");
    w->port = portbuf[0] ? g_strdup(portbuf) : NULL;

    g_thread_new("send-wifi", send_wifi_thread, w);
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
    gboolean wireless_ok = http_test_connection(app);
    if (!wireless_ok) {
        app->camera_status = gtk_label_new("No camera connected wirelessly");
    } else {
        app->camera_status = gtk_label_new("Connected to camera wirelessly");
    }
    gtk_box_pack_start(GTK_BOX(main_box), app->camera_status, FALSE, FALSE, 0);

    // Serial port status (will be updated asynchronously)
    app->serial_status_label = gtk_label_new("Searching for camera...");
    gtk_box_pack_start(GTK_BOX(main_box), app->serial_status_label, FALSE, FALSE, 0);

    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 10);

    // Setup button
    app->setup_button = gtk_button_new_with_label("Setup Camera");
    g_signal_connect(app->setup_button, "clicked", G_CALLBACK(on_setup_camera_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), app->setup_button, FALSE, FALSE, 0);

    // Stream button
    app->stream_button_main = gtk_button_new_with_label("Open Stream");
    if (app->streaming) {
        gtk_button_set_label(GTK_BUTTON(app->stream_button_main), "Stop Stream");
    }
    g_signal_connect(app->stream_button_main, "clicked", G_CALLBACK(on_rotate_clicked), app);
    gtk_box_pack_start(GTK_BOX(button_box), app->stream_button_main, FALSE, FALSE, 0);
    // Disable stream buttons if no wireless camera connection
    gtk_widget_set_sensitive(app->stream_button_main, wireless_ok);

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
