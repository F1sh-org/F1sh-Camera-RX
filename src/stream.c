#include "f1sh_camera_rx.h"

static gboolean bus_callback(GstBus *bus __attribute__((unused)), GstMessage *message, gpointer user_data) {
    App *app = (App*)user_data;
    
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error;
            gchar *debug;
            gst_message_parse_error(message, &error, &debug);
            ui_log(app, "GStreamer Error: %s", error->message);
            ui_log(app, "Debug info: %s", debug ? debug : "none");
            g_error_free(error);
            g_free(debug);
            ui_update_status(app, "Stream Error");
            break;
        }
        case GST_MESSAGE_EOS:
            ui_log(app, "End of stream");
            ui_update_status(app, "Stream Ended");
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(app->pipeline)) {
                ui_log(app, "Pipeline state changed from %s to %s",
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
    if (!have_flip && app->config.rotate != 0) {
        ui_log(app, "Warning: GStreamer element 'videoflip' not found. Rotation disabled.");
        app->config.rotate = 0;
    }
    switch (app->config.rotate) {
        case 1: flip = have_flip ? " ! videoflip method=clockwise ! videoconvert" : ""; break;
        case 2: flip = have_flip ? " ! videoflip method=rotate-180 ! videoconvert" : ""; break;
        case 3: flip = have_flip ? " ! videoflip method=counterclockwise ! videoconvert" : ""; break;
        default: flip = ""; break;
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

    ui_log(app, "Creating pipeline: %s", pipeline_desc);

    GError *error = NULL;
    app->pipeline = gst_parse_launch(pipeline_desc, &error);

    // Fallback: software decoder with autovideosink
    if (!app->pipeline || error) {
        ui_log(app, "HW pipeline failed, falling back to software decoder");
        if (error) {
            ui_log(app, "HW pipeline error: %s", error->message);
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

        ui_log(app, "Creating fallback pipeline: %s", pipeline_desc);
        app->pipeline = gst_parse_launch(pipeline_desc, &error);
    }
    
    if (!app->pipeline || error) {
        ui_log(app, "Failed to create pipeline: %s", error ? error->message : "Unknown error");
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
        ui_log(app, "Failed to start pipeline");
        gst_object_unref(app->pipeline);
        app->pipeline = NULL;
        return FALSE;
    }
    
    app->streaming = TRUE;
    ui_log(app, "Low-latency stream started, listening on UDP port %d", app->config.rx_stream_port);
    return TRUE;
}

void stream_stop(App *app) {
    if (app->bus_watch_id != 0) {
        g_source_remove(app->bus_watch_id);
        app->bus_watch_id = 0;
    }
    if (app->pipeline) {
        ui_log(app, "Stopping stream");
        gst_element_set_state(app->pipeline, GST_STATE_NULL);
        gst_object_unref(app->pipeline);
        app->pipeline = NULL;
    }
    app->streaming = FALSE;
}
