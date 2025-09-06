#include "f1sh_camera_rx.h"

static gboolean bus_callback(GstBus *bus __attribute__((unused)), GstMessage *message, gpointer user_data) {
    App *app = (App*)user_data;
    
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error;
            gchar *debug;
            gst_message_parse_error(message, &error, &debug);
            printf("GStreamer Error: %s\n", error->message);
            printf("Debug info: %s\n", debug ? debug : "none");
            g_error_free(error);
            g_free(debug);
            ui_update_status(app, "Stream Error");
            break;
        }
        case GST_MESSAGE_EOS:
            printf("End of stream\n");
            ui_update_status(app, "Stream Ended");
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(app->pipeline)) {
                printf("Pipeline state changed from %s to %s\n",
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
    
    // Create low-latency pipeline with optimized settings
    char pipeline_desc[1024];
    snprintf(pipeline_desc, sizeof(pipeline_desc),
             "udpsrc port=%d buffer-size=65536 ! "
             "application/x-rtp,encoding-name=H264,payload=96 ! "
             "rtph264depay ! "
             "h264parse ! "
             "vtdec_hw ! "  // Hardware decoder on macOS, fallback to avdec_h264 if unavailable
             "videoconvert ! "
             "queue max-size-buffers=1 leaky=downstream ! "
             "autovideosink sync=false",
             app->config.rx_stream_port);
    
    printf("Creating low-latency pipeline with hardware decoding: %s\n", pipeline_desc);
    
    GError *error = NULL;
    app->pipeline = gst_parse_launch(pipeline_desc, &error);
    
    // If hardware decoder fails, fall back to software decoder
    if (!app->pipeline || error) {
        printf("Hardware decoder failed, falling back to software decoder\n");
        if (error) {
            printf("Hardware decoder error: %s\n", error->message);
            g_error_free(error);
            error = NULL;
        }
        
        snprintf(pipeline_desc, sizeof(pipeline_desc),
                 "udpsrc port=%d buffer-size=65536 ! "
                 "application/x-rtp,encoding-name=H264,payload=96 ! "
                 "rtph264depay ! "
                 "h264parse ! "
                 "avdec_h264 max-threads=1 ! "
                 "videoconvert ! "
                 "queue max-size-buffers=1 leaky=downstream ! "
                 "autovideosink sync=false",
                 app->config.rx_stream_port);
        
        printf("Creating fallback pipeline: %s\n", pipeline_desc);
        app->pipeline = gst_parse_launch(pipeline_desc, &error);
    }
    
    if (!app->pipeline || error) {
        printf("Failed to create pipeline: %s\n", error ? error->message : "Unknown error");
        if (error) g_error_free(error);
        return FALSE;
    }
    
    // Set low-latency properties on the pipeline
    GstElement *udpsrc = gst_bin_get_by_name(GST_BIN(app->pipeline), "udpsrc0");
    if (udpsrc) {
        // Minimize buffering for lowest latency
        g_object_set(udpsrc, 
                     "buffer-size", 65536,
                     "timeout", 1000000,  // 1ms timeout
                     NULL);
        gst_object_unref(udpsrc);
    }
    
    // Configure RTP jitter buffer for minimal latency
    GstElement *rtpjitterbuffer = gst_bin_get_by_name(GST_BIN(app->pipeline), "rtpjitterbuffer0");
    if (rtpjitterbuffer) {
        g_object_set(rtpjitterbuffer,
                     "latency", 50,           // 50ms max latency
                     "drop-on-latency", TRUE, // Drop late packets
                     "mode", 1,               // Slave mode for sync
                     NULL);
        gst_object_unref(rtpjitterbuffer);
    }
    
    // Set up bus
    GstBus *bus = gst_element_get_bus(app->pipeline);
    gst_bus_add_watch(bus, bus_callback, app);
    gst_object_unref(bus);
    
    // Set low-latency mode on the entire pipeline
    gst_pipeline_set_latency(GST_PIPELINE(app->pipeline), 50 * GST_MSECOND); // 50ms max latency
    
    // Start pipeline
    GstStateChangeReturn ret = gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        printf("Failed to start pipeline\n");
        gst_object_unref(app->pipeline);
        app->pipeline = NULL;
        return FALSE;
    }
    
    app->streaming = TRUE;
    printf("Low-latency stream started, listening on UDP port %d\n", app->config.rx_stream_port);
    return TRUE;
}

void stream_stop(App *app) {
    if (app->pipeline) {
        printf("Stopping stream\n");
        gst_element_set_state(app->pipeline, GST_STATE_NULL);
        gst_object_unref(app->pipeline);
        app->pipeline = NULL;
    }
    app->streaming = FALSE;
}
