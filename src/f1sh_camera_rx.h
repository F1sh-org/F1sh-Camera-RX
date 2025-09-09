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
} Config;

// Application state
typedef struct {
    Config config;
    
    // GTK widgets
    GtkWidget *window;
    GtkWidget *tx_ip_entry;
    GtkWidget *tx_port_spin;
    GtkWidget *rx_ip_entry;
    GtkWidget *rx_port_spin;
    GtkWidget *width_spin;
    GtkWidget *height_spin;
    GtkWidget *framerate_spin;
    GtkWidget *status_label;
    GtkWidget *stream_button;
    
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
void ui_create(App *app);
void ui_update_status(App *app, const char *status);

// http_client.c
gboolean http_test_connection(App *app);
gboolean http_send_config(App *app);

// stream.c
gboolean stream_start(App *app);
void stream_stop(App *app);

#endif // F1SH_CAMERA_RX_H
