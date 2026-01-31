#include "streammanager.h"
#include "logmanager.h"
#include <QDebug>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>

// ============ VideoFrameProvider Implementation ============

VideoFrameProvider::VideoFrameProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage VideoFrameProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(id);
    Q_UNUSED(requestedSize);

    QMutexLocker locker(&m_mutex);

    if (m_currentFrame.isNull()) {
        // Return a placeholder image
        QImage placeholder(640, 480, QImage::Format_RGB32);
        placeholder.fill(Qt::black);
        if (size) *size = placeholder.size();
        return placeholder;
    }

    if (size) *size = m_currentFrame.size();
    return m_currentFrame;
}

void VideoFrameProvider::updateFrame(const QImage &frame)
{
    QMutexLocker locker(&m_mutex);
    m_currentFrame = frame;
}

// ============ StreamManager Implementation ============

StreamManager::StreamManager(QObject *parent)
    : QObject(parent)
    , m_imageProvider(new VideoFrameProvider())
    , m_frameTimer(new QTimer(this))
{
    initGStreamer();
    detectDecoders();

    // Setup frame polling timer (as fallback for callback issues)
    connect(m_frameTimer, &QTimer::timeout, this, &StreamManager::pollForFrames);
}

StreamManager::~StreamManager()
{
    stop();
    // Note: m_imageProvider is owned by QML engine after registration
}

void StreamManager::initGStreamer()
{
    if (m_gstInitialized) return;

    GError *error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &error)) {
        QString errorMsg = error ? QString::fromUtf8(error->message) : "Unknown error";
        LogManager::log(QString("Failed to initialize GStreamer: %1").arg(errorMsg));
        if (error) g_error_free(error);
        return;
    }

    m_gstInitialized = true;
    LogManager::log(QString("GStreamer initialized: %1").arg(gst_version_string()));
}

void StreamManager::detectDecoders()
{
    m_decoders.clear();

    // Define all possible H.264 decoders with their priorities
    struct DecoderCandidate {
        const char *name;
        const char *element;
        const char *description;
        bool isHardware;
        int priority;
    };

    // Platform-specific decoder preferences
    QList<DecoderCandidate> candidates;

#ifdef _WIN32
    // Windows: Prefer D3D11/D3D12 hardware decoders
    candidates = {
        {"D3D12 H.264", "d3d12h264dec", "DirectX 12 Hardware Decoder", true, 100},
        {"D3D11 H.264", "d3d11h264dec", "DirectX 11 Hardware Decoder", true, 95},
        {"NVDEC H.264", "nvh264dec", "NVIDIA Hardware Decoder", true, 90},
        {"Intel QuickSync", "qsvh264dec", "Intel QuickSync Decoder", true, 85},
        {"FFmpeg H.264", "avdec_h264", "Software Decoder (FFmpeg)", false, 10},
        {"OpenH264", "openh264dec", "Software Decoder (OpenH264)", false, 5},
    };
#elif defined(__APPLE__)
    // macOS: Prefer VideoToolbox
    candidates = {
        {"VideoToolbox HW", "vtdec_hw", "Apple Hardware Decoder", true, 100},
        {"VideoToolbox", "vtdec", "Apple VideoToolbox", true, 95},
        {"FFmpeg H.264", "avdec_h264", "Software Decoder (FFmpeg)", false, 10},
    };
#else
    // Linux: Check for VA-API, NVDEC, V4L2
    candidates = {
        {"VA-API H.264", "vaapih264dec", "VA-API Hardware Decoder", true, 100},
        {"NVDEC H.264", "nvh264dec", "NVIDIA Hardware Decoder", true, 95},
        {"V4L2 H.264", "v4l2h264dec", "V4L2 Hardware Decoder", true, 90},
        {"FFmpeg H.264", "avdec_h264", "Software Decoder (FFmpeg)", false, 10},
        {"OpenH264", "openh264dec", "Software Decoder (OpenH264)", false, 5},
    };
#endif

    // Check which decoders are actually available
    for (const auto &candidate : candidates) {
        GstElementFactory *factory = gst_element_factory_find(candidate.element);
        if (factory) {
            DecoderInfo info;
            info.name = QString::fromUtf8(candidate.name);
            info.elementName = QString::fromUtf8(candidate.element);
            info.description = QString::fromUtf8(candidate.description);
            info.isHardware = candidate.isHardware;
            info.priority = candidate.priority;

            m_decoders.append(info);
            LogManager::log(QString("Found decoder: %1 (%2) - %3")
                           .arg(info.name, info.elementName,
                                info.isHardware ? "Hardware" : "Software"));

            gst_object_unref(factory);
        }
    }

    if (m_decoders.isEmpty()) {
        LogManager::log("WARNING: No H.264 decoders found!");
    }

    emit availableDecodersChanged();
}

QStringList StreamManager::availableDecoders() const
{
    QStringList result;
    for (const auto &decoder : m_decoders) {
        result.append(decoder.name);
    }
    return result;
}

DecoderInfo StreamManager::selectBestDecoder()
{
    // If user has a preference, try to use it
    if (!m_preferredDecoder.isEmpty()) {
        for (const auto &decoder : m_decoders) {
            if (decoder.name == m_preferredDecoder || decoder.elementName == m_preferredDecoder) {
                LogManager::log(QString("Using preferred decoder: %1").arg(decoder.name));
                return decoder;
            }
        }
        LogManager::log(QString("Preferred decoder '%1' not available, auto-selecting...").arg(m_preferredDecoder));
    }

    // Sort by priority and pick the best
    DecoderInfo best;
    best.priority = -1;

    for (const auto &decoder : m_decoders) {
        if (decoder.priority > best.priority) {
            best = decoder;
        }
    }

    if (best.priority >= 0) {
        LogManager::log(QString("Auto-selected decoder: %1 (%2)")
                       .arg(best.name, best.isHardware ? "Hardware" : "Software"));
    }

    return best;
}

void StreamManager::setPreferredDecoder(const QString &decoderName)
{
    m_preferredDecoder = decoderName;
    LogManager::log(QString("Preferred decoder set to: %1").arg(decoderName));
}

QString StreamManager::buildPipelineString()
{
    DecoderInfo decoder = selectBestDecoder();
    if (decoder.elementName.isEmpty()) {
        LogManager::log("No decoder available!");
        return QString();
    }

    m_currentDecoder = decoder.name;
    emit currentDecoderChanged();

    QString pipeline;

    // UDP source - minimal buffering for low latency
    pipeline = QString("udpsrc port=%1 ! "
                       "application/x-rtp,media=video,encoding-name=H264,payload=96 ! "
                       "rtph264depay ! "
                       "h264parse ! ").arg(m_port);

    // Add decoder
    pipeline += decoder.elementName + " ! ";

    // Platform-specific post-processing
#ifdef _WIN32
    if (decoder.elementName.startsWith("d3d11") || decoder.elementName.startsWith("d3d12")) {
        // D3D11/D3D12 path: convert and download to system memory
        if (decoder.elementName.startsWith("d3d12")) {
            pipeline += "d3d12download ! ";
        } else {
            pipeline += "d3d11download ! ";
        }
    }
#endif

    // Video conversion for rotation and format
    pipeline += "videoconvert ! ";

    // Add rotation if needed
    if (m_rotate != 0) {
        GstElementFactory *flipFactory = gst_element_factory_find("videoflip");
        if (flipFactory) {
            QString method;
            switch (m_rotate) {
                case 1: method = "clockwise"; break;
                case 2: method = "rotate-180"; break;
                case 3: method = "counterclockwise"; break;
                default: method = ""; break;
            }
            if (!method.isEmpty()) {
                pipeline += QString("videoflip method=%1 ! videoconvert ! ").arg(method);
            }
            gst_object_unref(flipFactory);
        } else {
            LogManager::log("Warning: videoflip element not available, rotation disabled");
        }
    }

    // Convert to RGB for Qt and use appsink
    pipeline += "video/x-raw,format=RGB ! "
                "queue max-size-buffers=3 leaky=downstream ! "
                "appsink name=sink emit-signals=true sync=false max-buffers=3 drop=true";

    LogManager::log(QString("Pipeline: %1").arg(pipeline));
    return pipeline;
}

bool StreamManager::createPipeline()
{
    QString pipelineStr = buildPipelineString();
    if (pipelineStr.isEmpty()) {
        setStatus("No decoder available");
        return false;
    }

    LogManager::log(QString("Creating pipeline: %1").arg(pipelineStr));

    GError *error = nullptr;
    m_pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);

    if (!m_pipeline || error) {
        QString errorMsg = error ? QString::fromUtf8(error->message) : "Unknown error";
        LogManager::log(QString("Failed to create pipeline: %1").arg(errorMsg));
        if (error) g_error_free(error);

        // Try fallback with software decoder
        if (m_currentDecoder != "FFmpeg H.264") {
            LogManager::log("Attempting fallback to software decoder...");
            m_preferredDecoder = "avdec_h264";
            return createPipeline();
        }

        setStatus("Failed to create pipeline");
        return false;
    }

    // Get appsink element
    m_appSink = gst_bin_get_by_name(GST_BIN(m_pipeline), "sink");
    if (!m_appSink) {
        LogManager::log("Failed to get appsink element");
        destroyPipeline();
        setStatus("Pipeline configuration error");
        return false;
    }

    // Configure appsink callbacks
    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = onNewSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(m_appSink), &callbacks, this, nullptr);

    // Set up bus watch
    GstBus *bus = gst_element_get_bus(m_pipeline);
    m_busWatchId = gst_bus_add_watch(bus, onBusMessage, this);
    gst_object_unref(bus);

    // Set low latency
    gst_pipeline_set_latency(GST_PIPELINE(m_pipeline), 50 * GST_MSECOND);

    return true;
}

void StreamManager::destroyPipeline()
{
    if (m_busWatchId != 0) {
        g_source_remove(m_busWatchId);
        m_busWatchId = 0;
    }

    if (m_appSink) {
        gst_object_unref(m_appSink);
        m_appSink = nullptr;
    }

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}

void StreamManager::start()
{
    if (m_isStreaming) {
        LogManager::log("Stream already running");
        return;
    }

    if (!m_gstInitialized) {
        initGStreamer();
        if (!m_gstInitialized) {
            setStatus("GStreamer not initialized");
            emit errorOccurred("GStreamer initialization failed");
            return;
        }
    }

    LogManager::log(QString("Starting stream on UDP port %1...").arg(m_port));
    setStatus("Starting...");

    if (!createPipeline()) {
        emit errorOccurred("Failed to create pipeline");
        return;
    }

    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LogManager::log("Failed to start pipeline");
        destroyPipeline();
        setStatus("Failed to start");
        emit errorOccurred("Failed to start pipeline");
        return;
    }

    m_isStreaming = true;
    emit isStreamingChanged();
    setStatus(QString("Streaming (%1)").arg(m_currentDecoder));
    LogManager::log(QString("Stream started on port %1 using %2").arg(m_port).arg(m_currentDecoder));

    // Start frame polling timer (30fps polling rate)
    m_frameTimer->start(33);
}

void StreamManager::stop()
{
    // Stop frame timer first
    if (m_frameTimer) {
        m_frameTimer->stop();
    }

    if (!m_isStreaming && !m_pipeline) {
        return;
    }

    LogManager::log("Stopping stream...");
    setStatus("Stopping...");

    destroyPipeline();

    m_isStreaming = false;
    emit isStreamingChanged();
    setStatus("Stopped");
    LogManager::log("Stream stopped");
}

void StreamManager::setPort(int port)
{
    if (m_port != port) {
        m_port = port;
        emit portChanged();

        // If streaming, restart with new port
        if (m_isStreaming) {
            stop();
            start();
        }
    }
}

void StreamManager::setRotate(int rotate)
{
    rotate = qBound(0, rotate, 3);
    if (m_rotate != rotate) {
        m_rotate = rotate;
        emit rotateChanged();

        // If streaming, restart with new rotation
        if (m_isStreaming) {
            stop();
            start();
        }
    }
}

void StreamManager::setStatus(const QString &status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void StreamManager::pollForFrames()
{
    if (!m_appSink || !m_isStreaming) {
        return;
    }

    // Try to pull a sample from appsink (non-blocking)
    GstSample *sample = gst_app_sink_try_pull_sample(GST_APP_SINK(m_appSink), 0);
    if (!sample) {
        return;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);

    if (buffer && caps) {
        GstVideoInfo videoInfo;
        if (gst_video_info_from_caps(&videoInfo, caps)) {
            GstMapInfo mapInfo;
            if (gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
                // Get proper stride from video info
                int stride = GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0);

                // Create QImage from RGB data
                QImage frame(mapInfo.data,
                            videoInfo.width,
                            videoInfo.height,
                            stride,
                            QImage::Format_RGB888);

                // Make a deep copy since the buffer will be released
                m_imageProvider->updateFrame(frame.copy());

                // Log first frame received
                static bool firstFrame = true;
                if (firstFrame) {
                    LogManager::log(QString("First frame received (poll): %1x%2, stride=%3")
                                   .arg(videoInfo.width).arg(videoInfo.height).arg(stride));
                    firstFrame = false;
                }

                emit frameReady();

                gst_buffer_unmap(buffer, &mapInfo);
            }
        }
    }

    gst_sample_unref(sample);
}

// GStreamer callback: new video sample available
GstFlowReturn StreamManager::onNewSample(GstAppSink *sink, gpointer userData)
{
    StreamManager *self = static_cast<StreamManager*>(userData);

    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_OK;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);

    if (buffer && caps) {
        GstVideoInfo videoInfo;
        if (gst_video_info_from_caps(&videoInfo, caps)) {
            GstMapInfo mapInfo;
            if (gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
                // Get proper stride from video info
                int stride = GST_VIDEO_INFO_PLANE_STRIDE(&videoInfo, 0);

                // Create QImage from RGB data
                QImage frame(mapInfo.data,
                            videoInfo.width,
                            videoInfo.height,
                            stride,
                            QImage::Format_RGB888);

                // Make a deep copy since the buffer will be released
                self->m_imageProvider->updateFrame(frame.copy());

                // Log first frame received
                static bool firstFrame = true;
                if (firstFrame) {
                    LogManager::log(QString("First frame received: %1x%2, stride=%3")
                                   .arg(videoInfo.width).arg(videoInfo.height).arg(stride));
                    firstFrame = false;
                }

                emit self->frameReady();

                gst_buffer_unmap(buffer, &mapInfo);
            }
        }
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// GStreamer callback: bus messages
gboolean StreamManager::onBusMessage(GstBus *bus, GstMessage *message, gpointer userData)
{
    Q_UNUSED(bus);
    StreamManager *self = static_cast<StreamManager*>(userData);

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_error(message, &error, &debug);

            QString errorMsg = error ? QString::fromUtf8(error->message) : "Unknown error";
            LogManager::log(QString("GStreamer Error: %1").arg(errorMsg));
            if (debug) {
                LogManager::log(QString("Debug: %1").arg(QString::fromUtf8(debug)));
            }

            self->setStatus(QString("Error: %1").arg(errorMsg));
            emit self->errorOccurred(errorMsg);

            if (error) g_error_free(error);
            if (debug) g_free(debug);
            break;
        }

        case GST_MESSAGE_EOS:
            LogManager::log("End of stream");
            self->setStatus("Stream ended");
            break;

        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->m_pipeline)) {
                GstState oldState, newState;
                gst_message_parse_state_changed(message, &oldState, &newState, nullptr);
                LogManager::log(QString("Pipeline state: %1 -> %2")
                               .arg(QString::fromUtf8(gst_element_state_get_name(oldState)),
                                    QString::fromUtf8(gst_element_state_get_name(newState))));

                if (newState == GST_STATE_PLAYING) {
                    self->setStatus(QString("Streaming (%1)").arg(self->m_currentDecoder));
                }
            }
            break;
        }

        case GST_MESSAGE_WARNING: {
            GError *warning = nullptr;
            gchar *debug = nullptr;
            gst_message_parse_warning(message, &warning, &debug);

            if (warning) {
                LogManager::log(QString("GStreamer Warning: %1").arg(QString::fromUtf8(warning->message)));
                g_error_free(warning);
            }
            if (debug) g_free(debug);
            break;
        }

        default:
            break;
    }

    return TRUE;
}
