#include "f1sh_camera_rx.h"
#include <sys/param.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

static void setup_gstreamer_env(void) {
#if defined(__APPLE__)
    // Try to locate the .app bundle structure
    char exe_path[PATH_MAX];
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        // exe: MyApp.app/Contents/MacOS/f1sh-camera-rx
        char bundle_root[PATH_MAX] = {0};
        strncpy(bundle_root, exe_path, sizeof(bundle_root) - 1);
        // Go up two levels to Contents
        char *p = strrchr(bundle_root, '/');
        if (p) { *p = '\0'; p = strrchr(bundle_root, '/'); }
        if (p) { *p = '\0'; }

        char frameworks[PATH_MAX];
        char resources[PATH_MAX];
        snprintf(frameworks, sizeof(frameworks), "%s/Frameworks", bundle_root);
        snprintf(resources, sizeof(resources), "%s/Resources", bundle_root);

    // If GStreamer.framework is embedded, point GST_PLUGIN_SYSTEM_PATH there
        char gst_plugins_from_framework[PATH_MAX];
        snprintf(gst_plugins_from_framework, sizeof(gst_plugins_from_framework),
                 "%s/GStreamer.framework/Versions/Current/lib/gstreamer-1.0", frameworks);
    char gst_scanner_from_framework[PATH_MAX];
    snprintf(gst_scanner_from_framework, sizeof(gst_scanner_from_framework),
         "%s/GStreamer.framework/Versions/Current/libexec/gstreamer-1.0/gst-plugin-scanner", frameworks);

        // Or if plugins are staged under Resources/gstreamer-1.0
    char gst_plugins_from_resources[PATH_MAX];
        snprintf(gst_plugins_from_resources, sizeof(gst_plugins_from_resources),
                 "%s/gstreamer-1.0", resources);
    char gst_scanner_from_resources[PATH_MAX];
    snprintf(gst_scanner_from_resources, sizeof(gst_scanner_from_resources),
         "%s/gst-plugin-scanner", resources);

        // Prefer embedded framework path if exists
        if (g_file_test(gst_plugins_from_framework, G_FILE_TEST_IS_DIR)) {
            g_setenv("GST_PLUGIN_SYSTEM_PATH", gst_plugins_from_framework, TRUE);
            if (g_file_test(gst_scanner_from_framework, G_FILE_TEST_IS_EXECUTABLE)) {
                g_setenv("GST_PLUGIN_SCANNER", gst_scanner_from_framework, TRUE);
            }
        } else if (g_file_test(gst_plugins_from_resources, G_FILE_TEST_IS_DIR)) {
            g_setenv("GST_PLUGIN_SYSTEM_PATH", gst_plugins_from_resources, TRUE);
            if (g_file_test(gst_scanner_from_resources, G_FILE_TEST_IS_EXECUTABLE)) {
                g_setenv("GST_PLUGIN_SCANNER", gst_scanner_from_resources, TRUE);
            }
        }

        // Use a user-writable registry cache to avoid writing inside bundle
        const char *cache_dir = g_get_user_cache_dir();
        if (cache_dir) {
            char reg[PATH_MAX];
            snprintf(reg, sizeof(reg), "%s/f1sh-camera-rx/gstreamer-registry.bin", cache_dir);
            g_setenv("GST_REGISTRY", reg, TRUE);
        }
    }
#elif defined(_WIN32)
    // Determine the directory of the executable
    wchar_t wexe[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, wexe, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        // Convert to UTF-8 for GLib helpers
        gchar *exe_u8 = g_utf16_to_utf8((const gunichar2*)wexe, -1, NULL, NULL, NULL);
        if (exe_u8) {
            gchar *dir = g_path_get_dirname(exe_u8);
            // Candidate plugin/scanner locations relative to exe
            gchar *plugins1 = g_build_filename(dir, "gstreamer-1.0", NULL);
            gchar *plugins2 = g_build_filename(dir, "lib", "gstreamer-1.0", NULL);
            gchar *scanner1 = g_build_filename(dir, "gst-plugin-scanner.exe", NULL);
            gchar *scanner2 = g_build_filename(dir, "libexec", "gstreamer-1.0", "gst-plugin-scanner.exe", NULL);
            // GTK / GSettings / GDK pixbuf assets
            gchar *schemas_dir = g_build_filename(dir, "share", "glib-2.0", "schemas", NULL);
            gchar *pixbuf_cache = g_build_filename(dir, "lib", "gdk-pixbuf-2.0", "2.10.0", "loaders.cache", NULL);

            const gchar *plugins_final = NULL;
            const gchar *scanner_final = NULL;
            if (g_file_test(plugins1, G_FILE_TEST_IS_DIR)) plugins_final = plugins1;
            else if (g_file_test(plugins2, G_FILE_TEST_IS_DIR)) plugins_final = plugins2;

            if (g_file_test(scanner1, G_FILE_TEST_IS_EXECUTABLE)) scanner_final = scanner1;
            else if (g_file_test(scanner2, G_FILE_TEST_IS_EXECUTABLE)) scanner_final = scanner2;

            if (plugins_final) g_setenv("GST_PLUGIN_SYSTEM_PATH", plugins_final, TRUE);
            if (scanner_final) g_setenv("GST_PLUGIN_SCANNER", scanner_final, TRUE);

            // Point GSettings to bundled compiled schemas (generated during packaging)
            if (g_file_test(schemas_dir, G_FILE_TEST_IS_DIR)) {
                g_setenv("GSETTINGS_SCHEMA_DIR", schemas_dir, TRUE);
            }
            // Point GDK pixbuf to bundled loader cache if present
            if (g_file_test(pixbuf_cache, G_FILE_TEST_EXISTS)) {
                g_setenv("GDK_PIXBUF_MODULE_FILE", pixbuf_cache, TRUE);
            }

            // Ensure our exe dir is on DLL search path so co-located DLLs are found
            SetDllDirectoryW(wexe); // NOTE: this sets to full path; adjust to directory below
            // Correct SetDllDirectory to directory, not file name
            wchar_t wdir[MAX_PATH];
            lstrcpynW(wdir, wexe, MAX_PATH);
            wchar_t *last_bs = wcsrchr(wdir, L'\\');
            if (last_bs) { *last_bs = L'\0'; SetDllDirectoryW(wdir); }

            // Set registry cache path
            const char *cache_dir = g_get_user_cache_dir();
            if (cache_dir) {
                char reg[PATH_MAX];
                snprintf(reg, sizeof(reg), "%s/f1sh-camera-rx/gstreamer-registry.bin", cache_dir);
                g_setenv("GST_REGISTRY", reg, TRUE);
            }

            g_free(plugins1);
            g_free(plugins2);
            g_free(scanner1);
            g_free(scanner2);
            g_free(schemas_dir);
            g_free(pixbuf_cache);
            g_free(dir);
            g_free(exe_u8);
        }
    }
#endif
}

void app_init(App *app) {
    // Initialize config with defaults
    app->config.tx_server_ip = g_strdup("127.0.0.1");
    app->config.tx_http_port = 8888;
    app->config.rx_host_ip = g_strdup("127.0.0.1");
    app->config.rx_stream_port = 5000;
    app->config.width = 1280;
    app->config.height = 720;
    app->config.framerate = 30;
    app->config.rotate = 0;
    
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

    // Stop HTTP server
    http_server_stop(app);
    
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
    setup_gstreamer_env();
    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Create application
    App app = {0};
    app_init(&app);
    
    // Create UI
    ui_main(&app);

    // Start local HTTP server (port 8889)
    http_server_start(&app);
    
    printf("F1sh Camera RX started\n");
    printf("Ready to connect to TX server\n");
    
    // Run main loop
    gtk_main();

    // Clean up cURL global (only at program exit)
    curl_global_cleanup();

    return 0;
}
