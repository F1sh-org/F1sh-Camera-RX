#include "f1sh_camera_rx.h"
#include <microhttpd.h>
#include <pthread.h>

#define RX_HTTP_PORT 8889

static gboolean restart_pipeline_idle(gpointer data)
{
    App *a = (App*)data;
    stream_stop(a);
    stream_start(a);
    return FALSE; // one-shot
}

typedef struct {
    App *app;
    struct MHD_Daemon *daemon;
    pthread_t thread;
    gboolean running;
} HttpServer;

// We store the server instance in a static variable; one per process
static HttpServer g_server = {0};

typedef struct {
    char *data;
    size_t size;
} PostData;

static enum MHD_Result send_json_response(struct MHD_Connection *connection, unsigned int status_code, const char *json)
{
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json), (void*)json, MHD_RESPMEM_MUST_COPY);
    if (!response) return MHD_NO;
    MHD_add_response_header(response, "Content-Type", "application/json");
    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

static enum MHD_Result handle_rotate(App *app, const char *body, struct MHD_Connection *connection)
{
    if (!body) {
        const char *err = "{\"error\":\"missing body\"}";
        return send_json_response(connection, MHD_HTTP_BAD_REQUEST, err);
    }

    json_error_t error;
    json_t *root = json_loads(body, 0, &error);
    if (!root) {
        const char *err = "{\"error\":\"invalid json\"}";
        return send_json_response(connection, MHD_HTTP_BAD_REQUEST, err);
    }

    json_t *rotate_val = json_object_get(root, "rotate");
    if (!rotate_val || !json_is_integer(rotate_val)) {
        json_decref(root);
        const char *err = "{\"error\":\"rotate must be integer 0-3\"}";
        return send_json_response(connection, MHD_HTTP_BAD_REQUEST, err);
    }

    int r = (int)json_integer_value(rotate_val);
    if (r < 0 || r > 3) {
        json_decref(root);
        const char *err = "{\"error\":\"rotate must be 0..3\"}";
        return send_json_response(connection, MHD_HTTP_BAD_REQUEST, err);
    }

    app->config.rotate = r;

    // If streaming, restart pipeline on GTK main loop to apply rotation safely
    if (app->streaming) {
        g_idle_add(restart_pipeline_idle, app);
    }

    json_decref(root);
    return send_json_response(connection, MHD_HTTP_OK, "{\"status\":\"ok\"}");
}

static enum MHD_Result access_handler(void *cls, struct MHD_Connection *connection, const char *url,
                          const char *method, const char *version __attribute__((unused)),
                          const char *upload_data, size_t *upload_data_size, void **con_cls)
{
    HttpServer *server = (HttpServer*)cls;
    App *app = server->app;

    // Initialize per-connection POST buffer
    if (*con_cls == NULL) {
        PostData *pd = calloc(1, sizeof(PostData));
        *con_cls = (void*)pd;
        return MHD_YES;
    }

    PostData *pd = (PostData*)(*con_cls);

    if (strcasecmp(method, "POST") == 0) {
        if (*upload_data_size != 0) {
            // Accumulate POST data
            size_t old = pd->size;
            pd->data = realloc(pd->data, old + *upload_data_size + 1);
            memcpy(pd->data + old, upload_data, *upload_data_size);
            pd->size += *upload_data_size;
            pd->data[pd->size] = '\0';
            *upload_data_size = 0;
            return MHD_YES; // wait for more data/end
        }

        if (strcmp(url, "/rotate") == 0) {
            int ret = handle_rotate(app, pd->data, connection);
            if (pd->data) free(pd->data);
            free(pd);
            *con_cls = NULL;
            return ret;
        }

        // Unknown endpoint
        const char *err = "{\"error\":\"not found\"}";
        int ret = send_json_response(connection, MHD_HTTP_NOT_FOUND, err);
        if (pd->data) free(pd->data);
        free(pd);
        *con_cls = NULL;
        return ret;
    }

    // Only POST supported
    const char *err = "{\"error\":\"method not allowed\"}";
    int ret = send_json_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, err);
    if (pd) {
        if (pd->data) free(pd->data);
        free(pd);
        *con_cls = NULL;
    }
    return ret;
}

static void* server_thread(void *arg)
{
    HttpServer *server = (HttpServer*)arg;
    server->daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, RX_HTTP_PORT, NULL, NULL,
                                      (MHD_AccessHandlerCallback)access_handler, server, MHD_OPTION_END);
    if (!server->daemon) {
        ui_log(server->app, "Failed to start HTTP server on port %d", RX_HTTP_PORT);
        server->running = FALSE;
        return NULL;
    }
    ui_log(server->app, "RX HTTP server listening on port %d", RX_HTTP_PORT);

    // Keep thread alive while running
    while (server->running) {
        g_usleep(1000 * 100); // 100ms
    }

    MHD_stop_daemon(server->daemon);
    server->daemon = NULL;
    return NULL;
}

void http_server_start(App *app)
{
    if (g_server.running) return;
    memset(&g_server, 0, sizeof(g_server));
    g_server.app = app;
    g_server.running = TRUE;
    if (pthread_create(&g_server.thread, NULL, server_thread, &g_server) != 0) {
        ui_log(g_server.app, "Failed to create HTTP server thread");
        g_server.running = FALSE;
    }
}

void http_server_stop(App *app __attribute__((unused)))
{
    if (!g_server.running) return;
    g_server.running = FALSE;
    pthread_join(g_server.thread, NULL);
}
