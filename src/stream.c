#include "f1sh_camera_rx.h"
#include "logmanager.h"

static gboolean bus_callback(GstBus *bus __attribute__((unused)), GstMessage *message, gpointer user_data) {
    App *app = (App*)user_data;
    
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error;
            gchar *debug;
            gst_message_parse_error(message, &error, &debug);
            app_log_fmt("GStreamer Error: %s", error->message);
            app_log_fmt("Debug info: %s", debug ? debug : "none");
            g_error_free(error);
            g_free(debug);
            ui_update_status(app, "Stream Error");
            break;
        }
        case GST_MESSAGE_EOS:
            app_log("End of stream");
            ui_update_status(app, "Stream Ended");
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(app->pipeline)) {
                app_log_fmt("Pipeline state changed from %s to %s",
                       gst_element_state_get_name(old_state),
                       gst_element_state_get_name(new_state));
                
                if (new_state == GST_STATE_PLAYING) {
                    ui_update_status(app, "Streaming");
                }
            }
            break;
        }
        default:
            break;
    }
    
    return TRUE;
}

gboolean stream_start(App *app) {
    if (app->pipeline) {
        stream_stop(app);
    }

    // Create low-latency pipeline with platform-appropriate decoder/sink
    char pipeline_desc[1024];

    // First attempt: platform HW decoder
    const char *flip = "";
    GstElementFactory *vf = gst_element_factory_find("videoflip");
    gboolean have_flip = (vf != NULL);
    if (vf) gst_object_unref(vf);
    switch (app->config.rotate) {
        case 1: flip = have_flip ? " ! videoflip method=clockwise ! videoconvert" : ""; break;
        case 2: flip = have_flip ? " ! videoflip method=rotate-180 ! videoconvert" : ""; break;
        case 3: flip = have_flip ? " ! videoflip method=counterclockwise ! videoconvert" : ""; break;
        default: flip = ""; break;
    }
    if (!have_flip && app->config.rotate != 0) {
        app_log("Warning: GStreamer element 'videoflip' not found. Rotation disabled.");
        app->config.rotate = 0;
    }
#if defined(__APPLE__)
    snprintf(pipeline_desc, sizeof(pipeline_desc),
             "udpsrc port=%d buffer-size=65536 ! "
             "application/x-rtp,encoding-name=H264,payload=96 ! "
             "rtph264depay ! "
             "h264parse ! "
             "vtdec_hw ! "
             "videoconvert%s ! "
             "queue max-size-buffers=1 leaky=downstream ! "
             "autovideosink sync=false",
             app->config.rx_stream_port, flip);
#elif defined(G_OS_WIN32)
    // Prefer D3D11 decoder/sink on Windows
    if (app->config.rotate && have_flip) {
        // Insert d3d11download after d3d11convert for compatibility
        snprintf(pipeline_desc, sizeof(pipeline_desc),
                 "udpsrc port=%d buffer-size=65536 ! "
                 "application/x-rtp,encoding-name=H264,payload=96 ! "
                 "rtph264depay ! "
                 "h264parse ! "
                 "d3d11h264dec ! "
                 "d3d11convert ! d3d11download ! videoconvert ! videoflip method=%s ! videoconvert ! queue max-size-buffers=1 leaky=downstream ! d3d11videosink sync=false",
                 app->config.rx_stream_port,
                 app->config.rotate == 1 ? "clockwise" :
                 app->config.rotate == 2 ? "rotate-180" :
                 app->config.rotate == 3 ? "counterclockwise" : "none");
    } else {
        snprintf(pipeline_desc, sizeof(pipeline_desc),
                 "udpsrc port=%d buffer-size=65536 ! "
                 "application/x-rtp,encoding-name=H264,payload=96 ! "
                 "rtph264depay ! "
                 "h264parse ! "
                 "d3d11h264dec ! "
                 "d3d11convert ! d3d11download ! videoconvert ! queue max-size-buffers=1 leaky=downstream ! d3d11videosink sync=false",
                 app->config.rx_stream_port);
    }
#else
    // Generic attempt (e.g., vaapi/nvdec could be added if desired)
    snprintf(pipeline_desc, sizeof(pipeline_desc),
             "udpsrc port=%d buffer-size=65536 ! "
             "application/x-rtp,encoding-name=H264,payload=96 ! "
             "rtph264depay ! "
             "h264parse ! "
             "avdec_h264 ! "
             "videoconvert ! video/x-raw%s ! "
             "queue max-size-buffers=1 leaky=downstream ! "
             "autovideosink sync=false",
             app->config.rx_stream_port, flip);
#endif

    app_log_fmt("Creating pipeline: %s", pipeline_desc);

    GError *error = NULL;
    app->pipeline = gst_parse_launch(pipeline_desc, &error);

    // Fallback: software decoder with autovideosink
    if (!app->pipeline || error) {
        app_log("HW pipeline failed, falling back to software decoder");
        if (error) {
            app_log_fmt("HW pipeline error: %s", error->message);
            g_error_free(error);
            error = NULL;
        }

    snprintf(pipeline_desc, sizeof(pipeline_desc),
         "udpsrc port=%d buffer-size=65536 ! "
         "application/x-rtp,encoding-name=H264,payload=96 ! "
         "rtph264depay ! "
         "h264parse ! "
         "avdec_h264 ! "
         "videoconvert ! video/x-raw%s ! "
         "queue max-size-buffers=1 leaky=downstream ! "
         "autovideosink sync=false",
     app->config.rx_stream_port, flip);

        app_log_fmt("Creating fallback pipeline: %s", pipeline_desc);
        app->pipeline = gst_parse_launch(pipeline_desc, &error);
    }
    
    if (!app->pipeline || error) {
        app_log_fmt("Failed to create pipeline: %s", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        return FALSE;
    }
    
    // Optional: try to tweak udpsrc if auto-named (best-effort; ignore if not found)
    GstElement *udpsrc = gst_bin_get_by_name(GST_BIN(app->pipeline), "udpsrc0");
    if (udpsrc) {
        g_object_set(udpsrc, "buffer-size", 65536, NULL);
        gst_object_unref(udpsrc);
    }
    
    // Set up bus
    GstBus *bus = gst_element_get_bus(app->pipeline);
    if (app->bus_watch_id != 0) {
        g_source_remove(app->bus_watch_id);
        app->bus_watch_id = 0;
    }
    app->bus_watch_id = gst_bus_add_watch(bus, bus_callback, app);
    gst_object_unref(bus);
    
    // Set low-latency mode on the entire pipeline
    gst_pipeline_set_latency(GST_PIPELINE(app->pipeline), 50 * GST_MSECOND); // 50ms max latency
    
    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        app_log("Failed to start pipeline");
        gst_object_unref(app->pipeline);
        app->pipeline = NULL;
        return FALSE;
    }
    
    app->streaming = TRUE;
    app_log_fmt("Low-latency stream started, listening on UDP port %d", app->config.rx_stream_port);
    return TRUE;
}

void stream_stop(App *app) {
    if (app->bus_watch_id != 0) {
        g_source_remove(app->bus_watch_id);
        app->bus_watch_id = 0;
    }
    if (app->pipeline) {
        app_log("Stopping stream");
        gst_element_set_state(app->pipeline, GST_STATE_NULL);
        gst_object_unref(app->pipeline);
        app->pipeline = NULL;
    }
    app->streaming = FALSE;
}
