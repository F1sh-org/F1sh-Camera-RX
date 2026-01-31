#ifndef STREAMMANAGER_H
#define STREAMMANAGER_H

#include <QObject>
#include <QImage>
#include <QQuickImageProvider>
#include <QMutex>
#include <QTimer>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

// Forward declarations
class StreamManager;

// Image provider for QML to display video frames
class VideoFrameProvider : public QQuickImageProvider
{
public:
    VideoFrameProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    void updateFrame(const QImage &frame);

private:
    QImage m_currentFrame;
    QMutex m_mutex;
};

// Decoder info structure
struct DecoderInfo {
    QString name;
    QString elementName;
    QString description;
    bool isHardware;
    int priority; // Higher = prefer
};

class StreamManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isStreaming READ isStreaming NOTIFY isStreamingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString currentDecoder READ currentDecoder NOTIFY currentDecoderChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(int rotate READ rotate WRITE setRotate NOTIFY rotateChanged)
    Q_PROPERTY(QStringList availableDecoders READ availableDecoders NOTIFY availableDecodersChanged)

public:
    explicit StreamManager(QObject *parent = nullptr);
    ~StreamManager();

    bool isStreaming() const { return m_isStreaming; }
    QString status() const { return m_status; }
    QString currentDecoder() const { return m_currentDecoder; }
    int port() const { return m_port; }
    int rotate() const { return m_rotate; }
    QStringList availableDecoders() const;

    void setPort(int port);
    void setRotate(int rotate);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void detectDecoders();
    Q_INVOKABLE void setPreferredDecoder(const QString &decoderName);

    // Get the image provider for QML registration
    VideoFrameProvider* imageProvider() { return m_imageProvider; }

signals:
    void isStreamingChanged();
    void statusChanged();
    void currentDecoderChanged();
    void portChanged();
    void rotateChanged();
    void availableDecodersChanged();
    void frameReady();
    void errorOccurred(const QString &error);

private:
    void initGStreamer();
    bool createPipeline();
    void destroyPipeline();
    QString buildPipelineString();
    DecoderInfo selectBestDecoder();
    void setStatus(const QString &status);
    void pollForFrames();

    // GStreamer callbacks
    static GstFlowReturn onNewSample(GstAppSink *sink, gpointer userData);
    static gboolean onBusMessage(GstBus *bus, GstMessage *message, gpointer userData);

    bool m_isStreaming = false;
    bool m_gstInitialized = false;
    QString m_status;
    QString m_currentDecoder;
    QString m_preferredDecoder;
    int m_port = 8888;
    int m_rotate = 0;

    GstElement *m_pipeline = nullptr;
    GstElement *m_appSink = nullptr;
    guint m_busWatchId = 0;

    QList<DecoderInfo> m_decoders;
    VideoFrameProvider *m_imageProvider = nullptr;
    QTimer *m_frameTimer = nullptr;
};

#endif // STREAMMANAGER_H
