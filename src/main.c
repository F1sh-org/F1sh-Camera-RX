#include "f1sh_camera_rx.h"

void app_init(App *app) {
    // Initialize config with defaults
    app->config.tx_server_ip = g_strdup("127.0.0.1");
    app->config.tx_http_port = 8888;
    app->config.rx_host_ip = g_strdup("127.0.0.1");
    app->config.rx_stream_port = 5000;
    app->config.width = 1280;
    app->config.height = 720;
    app->config.framerate = 30;
    
    // Initialize state
    app->connected = FALSE;
    app->streaming = FALSE;
    app->pipeline = NULL;
    app->bus_watch_id = 0;
    
    // Initialize cURL
    app->curl = curl_easy_init();
    if (!app->curl) {
        printf("Failed to initialize cURL\n");
        exit(1);
    }
    
    printf("Application initialized\n");
}

void app_cleanup(App *app) {
    printf("Cleaning up application\n");
    
    // Stop streaming
    stream_stop(app);
    
    // Cleanup cURL
    if (app->curl) {
        curl_easy_cleanup(app->curl);
        app->curl = NULL;
    }
    
    // Free config strings
    if (app->config.tx_server_ip) {
        g_free(app->config.tx_server_ip);
        app->config.tx_server_ip = NULL;
    }
    if (app->config.rx_host_ip) {
        g_free(app->config.rx_host_ip);
        app->config.rx_host_ip = NULL;
    }
}

int main(int argc, char *argv[]) {
    // Initialize libraries
    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Create application
    App app = {0};
    app_init(&app);
    
    // Create UI
    ui_create(&app);
    
    printf("F1sh Camera RX started\n");
    printf("Ready to connect to TX server\n");
    
    // Run main loop
    gtk_main();

    // Clean up cURL global (only at program exit)
    curl_global_cleanup();

    return 0;
}
