#include "f1sh_camera_rx.h"

// HTTP response structure
struct HttpResponse {
    char *data;
    size_t size;
    App *app; // reference to app for logging in callbacks
};

// Callback to write HTTP response data
static size_t write_callback(void *contents, size_t size, size_t nmemb, struct HttpResponse *response) {
    size_t total_size = size * nmemb;
    char *ptr = realloc(response->data, response->size + total_size + 1);
    
    if (!ptr) {
        ui_log(response ? response->app : NULL, "Failed to allocate memory for HTTP response");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, total_size);
    response->size += total_size;
    response->data[response->size] = 0;
    
    return total_size;
}

gboolean http_test_connection(App *app) {
    if (!app->curl || !app->config.tx_server_ip) {
        ui_log(app, "HTTP client not initialized or no server IP");
        return FALSE;
    }
    
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/health", 
             app->config.tx_server_ip, app->config.tx_http_port);
    
    ui_log(app, "Testing connection to: %s", url);
    
    struct HttpResponse response = {0};
    response.app = app;
    
    // Reset curl handle to clean state
    curl_easy_reset(app->curl);
    
    curl_easy_setopt(app->curl, CURLOPT_URL, url);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(app->curl, CURLOPT_TIMEOUT, 5L);
    
    CURLcode res = curl_easy_perform(app->curl);
    
    gboolean success = FALSE;
    if (res == CURLE_OK && response.data) {
        ui_log(app, "Response: %s", response.data);
        
        // Parse JSON response
        json_error_t error;
        json_t *root = json_loads(response.data, 0, &error);
        if (root) {
            json_t *status = json_object_get(root, "status");
            if (status && json_is_string(status)) {
                const char *status_str = json_string_value(status);
                if (strcmp(status_str, "healthy") == 0) {
                    success = TRUE;
                }
            }
            json_decref(root);
        }
    } else {
        ui_log(app, "HTTP request failed: %s", curl_easy_strerror(res));
    }
    
    if (response.data) {
        free(response.data);
    }
    
    return success;
}

gboolean http_send_config(App *app) {
    if (!app->curl || !app->config.tx_server_ip) {
        return FALSE;
    }
    
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/config", 
             app->config.tx_server_ip, app->config.tx_http_port);
    
    // Create JSON config
    json_t *config = json_object();
    json_object_set_new(config, "host", json_string(app->config.rx_host_ip));
    json_object_set_new(config, "port", json_integer(app->config.rx_stream_port));
    json_object_set_new(config, "width", json_integer(app->config.width));
    json_object_set_new(config, "height", json_integer(app->config.height));
    json_object_set_new(config, "framerate", json_integer(app->config.framerate));
    
    char *json_string = json_dumps(config, 0);
    json_decref(config);
    
    if (!json_string) {
        ui_log(app, "Failed to create JSON config");
        return FALSE;
    }
    
    ui_log(app, "Sending config to: %s", url);
    ui_log(app, "Config: %s", json_string);
    
    struct HttpResponse response = {0};
    response.app = app;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    // Reset curl handle to clean state
    curl_easy_reset(app->curl);
    
    curl_easy_setopt(app->curl, CURLOPT_URL, url);
    curl_easy_setopt(app->curl, CURLOPT_POSTFIELDS, json_string);
    curl_easy_setopt(app->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(app->curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(app->curl);
    
    curl_slist_free_all(headers);
    free(json_string);
    
    gboolean success = FALSE;
    if (res == CURLE_OK && response.data) {
        ui_log(app, "Config response: %s", response.data);
        success = TRUE; // Assume success if we got any response
    } else {
        ui_log(app, "Config request failed: %s", curl_easy_strerror(res));
    }
    
    if (response.data) {
        free(response.data);
    }
    
    return success;
}

gboolean http_send_rx_rotate(App *app, int rotate) {
    if (!app->curl) return FALSE;

    char url[128];
    snprintf(url, sizeof(url), "http://%s:8889/rotate", app->config.rx_host_ip);

    json_t *root = json_object();
    json_object_set_new(root, "rotate", json_integer(rotate));
    char *json_string = json_dumps(root, 0);
    json_decref(root);
    if (!json_string) return FALSE;

    struct HttpResponse response = {0};
    response.app = app;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_reset(app->curl);
    curl_easy_setopt(app->curl, CURLOPT_URL, url);
    curl_easy_setopt(app->curl, CURLOPT_POSTFIELDS, json_string);
    curl_easy_setopt(app->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(app->curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(app->curl);

    curl_slist_free_all(headers);
    free(json_string);

    gboolean success = FALSE;
    if (res == CURLE_OK) {
        success = TRUE;
    } else {
        ui_log(app, "Rotate POST failed: %s", curl_easy_strerror(res));
    }

    if (response.data) free(response.data);
    return success;
}

gboolean http_request_swap(App *app) {
    if (!app || !app->curl || !app->config.tx_server_ip) {
        ui_log(app, "Swap request skipped: missing TX IP or curl");
        return FALSE;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/swap", app->config.tx_server_ip, app->config.tx_http_port);

    ui_log(app, "Requesting TX rotate swap at: %s", url);

    struct HttpResponse response = {0};
    response.app = app;

    // Reset curl handle to clean state
    curl_easy_reset(app->curl);
    curl_easy_setopt(app->curl, CURLOPT_URL, url);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(app->curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(app->curl);
    long http_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(app->curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    if (response.data) free(response.data);

    if (res == CURLE_OK && http_code == 200) {
        ui_log(app, "TX rotate swap OK (HTTP %ld)", http_code);
        return TRUE;
    }

    ui_log(app, "TX rotate swap failed: curl=%s http=%ld", curl_easy_strerror(res), http_code);
    return FALSE;
}

gboolean http_request_noswap(App *app) {
    if (!app || !app->curl || !app->config.tx_server_ip) {
        ui_log(app, "No-swap request skipped: missing TX IP or curl");
        return FALSE;
    }

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/noswap", app->config.tx_server_ip, app->config.tx_http_port);

    ui_log(app, "Requesting TX rotate noswap at: %s", url);

    struct HttpResponse response = {0};
    response.app = app;

    curl_easy_reset(app->curl);
    curl_easy_setopt(app->curl, CURLOPT_URL, url);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(app->curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(app->curl);
    long http_code = 0;
    if (res == CURLE_OK) {
        curl_easy_getinfo(app->curl, CURLINFO_RESPONSE_CODE, &http_code);
    }

    if (response.data) free(response.data);

    if (res == CURLE_OK && http_code == 200) {
        ui_log(app, "TX rotate noswap OK (HTTP %ld)", http_code);
        return TRUE;
    }

    ui_log(app, "TX rotate noswap failed: curl=%s http=%ld", curl_easy_strerror(res), http_code);
    return FALSE;
}

// TX config retrieval is now handled via serial in ui_main.c; HTTP variant removed.
