#include "f1sh_camera_rx.h"

// HTTP response structure
struct HttpResponse {
    char *data;
    size_t size;
};

// Callback to write HTTP response data
static size_t write_callback(void *contents, size_t size, size_t nmemb, struct HttpResponse *response) {
    size_t total_size = size * nmemb;
    char *ptr = realloc(response->data, response->size + total_size + 1);
    
    if (!ptr) {
        printf("Failed to allocate memory for HTTP response\n");
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
        printf("HTTP client not initialized or no server IP\n");
        return FALSE;
    }
    
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/health", 
             app->config.tx_server_ip, app->config.tx_http_port);
    
    printf("Testing connection to: %s\n", url);
    
    struct HttpResponse response = {0};
    
    // Reset curl handle to clean state
    curl_easy_reset(app->curl);
    
    curl_easy_setopt(app->curl, CURLOPT_URL, url);
    curl_easy_setopt(app->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(app->curl, CURLOPT_TIMEOUT, 5L);
    
    CURLcode res = curl_easy_perform(app->curl);
    
    gboolean success = FALSE;
    if (res == CURLE_OK && response.data) {
        printf("Response: %s\n", response.data);
        
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
        printf("HTTP request failed: %s\n", curl_easy_strerror(res));
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
        printf("Failed to create JSON config\n");
        return FALSE;
    }
    
    printf("Sending config to: %s\n", url);
    printf("Config: %s\n", json_string);
    
    struct HttpResponse response = {0};
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
        printf("Config response: %s\n", response.data);
        success = TRUE; // Assume success if we got any response
    } else {
        printf("Config request failed: %s\n", curl_easy_strerror(res));
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
        printf("Rotate POST failed: %s\n", curl_easy_strerror(res));
    }

    if (response.data) free(response.data);
    return success;
}
