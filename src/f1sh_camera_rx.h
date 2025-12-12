#ifndef F1SH_CAMERA_RX_H
#define F1SH_CAMERA_RX_H

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>
#include <curl/curl.h>
#include <jansson.h>
#include <stdio.h>
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

// Application state
typedef struct {
    Config config;
    
    // GTK widgets
    GtkWidget *window;
    GtkWidget *login_window;
    GtkWidget *config_window;
    GtkWidget *tx_ip_entry;
    GtkWidget *rx_ip_entry;
    GtkWidget *resolution_combo;
    GtkWidget *framerate_combo;
    GtkWidget *rotate_spin;
    GtkWidget *config_status_label;
    GtkWidget *stream_button;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *camera_status;
    
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

// ui.c
void ui_main(App *app);
void ui_configuration(App *app);
void ui_login(App *app);
void ui_update_status(App *app, const char *status);

// http_client.c
gboolean http_test_connection(App *app);
gboolean http_send_config(App *app);
gboolean http_send_rx_rotate(App *app, int rotate);

// stream.c
gboolean stream_start(App *app);
void stream_stop(App *app);

// http_server.c
void http_server_start(App *app); // starts HTTP server on port 8889 in background thread
void http_server_stop(App *app);

#endif // F1SH_CAMERA_RX_H
