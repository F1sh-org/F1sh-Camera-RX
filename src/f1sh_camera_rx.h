#ifndef F1SH_CAMERA_RX_H
#define F1SH_CAMERA_RX_H

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <curl/curl.h>
#include <jansson.h>
#include <stdlib.h>
#include <string.h>

// Application configuration
typedef struct {
    char *tx_server_ip;
    int tx_http_port;
    char *rx_host_ip;
    int rx_stream_port;
    int width;
    int height;
    int framerate;
    int rotate; // 0: none, 1: 90, 2: 180, 3: 270
} Config;

typedef struct {
    char ssid[64];
    char password[128];
    int success; // 1 = success, 0 = failed
} WifiInfo;

typedef struct {
    char adapter_name[256];
    char ip[32];
    char subnet[32];
    char gateway[32];
    int success;
} NetworkInfo;

// Wi‑Fi helpers
WifiInfo get_wifi_info(void);
NetworkInfo get_network_info(void);

// Application state
typedef struct {
    Config config;
    
    // GTK widgets
    GtkWidget *window;
    GtkWidget *login_window;
    GtkWidget *config_window;
    GtkWidget *rotate_window;
    GtkWidget *wifi_window;
    GtkWidget *tx_ip_entry;
    GtkWidget *rx_ip_entry;
    GtkWidget *resolution_combo;
    GtkWidget *framerate_combo;
    GtkWidget *rotate_spin;
    GtkWidget *config_status_label;
    GtkWidget *stream_button_main;
    GtkWidget *stream_button_config;
    GtkWidget *setup_button;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *camera_status;
    GtkWidget *serial_status_label;
    GtkWidget *log_view;
    GtkWidget *log_interactive_switch;
    gboolean log_interactive;
    guint log_event_press_handler_id;
    guint log_event_release_handler_id;
    guint log_event_enter_handler_id;
    guint log_event_leave_handler_id;
    
    // GStreamer
    GstElement *pipeline;
    guint bus_watch_id; // source ID for bus watch to remove on stop
    
    // HTTP client
    CURL *curl;
    
    // State
    gboolean connected;
    gboolean streaming;
} App;

// Function declarations
// main.c
void app_init(App *app);
void app_cleanup(App *app);

// ui_main.c
void ui_main(App *app);
void on_widget_destroy(GtkWidget *widget, gpointer user_data);
void on_open_stream_clicked(GtkButton *button, gpointer user_data);
void ui_update_status(App *app, const char *status);
void on_setup_camera_clicked(GtkButton *button, gpointer user_data);

// ui_wifi.c
void ui_wifi_show_list(App *app, json_t *list);
void ui_send_wifi_credentials(App *app, const char *bssid, const char *pass);

// ui_configuration.c
void ui_configuration(App *app);
void ui_set_log_interactive(App *app, gboolean interactive);

// ui_login.c
void ui_login(App *app);

// ui_log.c
void ui_log(App *app, const char *format, ...);
void ui_log_flush_buffer(App *app);
void ui_log_discard_buffer(void);

// ui_rotate.c
void ui_rotate(App *app);

// http_client.c
gboolean http_test_connection(App *app);
gboolean http_send_config(App *app);
gboolean http_send_rx_rotate(App *app, int rotate);
gboolean http_send_ip_addr(App *app, const char *ip);
gboolean http_request_swap(App *app);
gboolean http_request_tx_config(App *app, json_t **out_config);

// serial_probe.c
char *serial_find_camera_port(App *app);

// stream.c
gboolean stream_start(App *app);
void stream_stop(App *app);

// http_server.c
void http_server_start(App *app); // starts HTTP server on port 8889 in background thread
void http_server_stop(App *app);

#endif // F1SH_CAMERA_RX_H
