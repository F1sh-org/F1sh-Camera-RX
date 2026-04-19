#ifndef GRPCMANAGER_H
#define GRPCMANAGER_H

#include <QObject>
#include <QString>
#include <QThread>
#include <memory>

// Forward declarations for gRPC types
namespace grpc {
    class Channel;
}

namespace f1sh_camera {
    class F1shCameraService;
}

// Worker class that runs gRPC operations on a background thread
class GrpcWorker : public QObject
{
    Q_OBJECT
public:
    explicit GrpcWorker(QObject *parent = nullptr);
    ~GrpcWorker();

public slots:
    void doHealthCheck(const QString &serverAddress);
    void doGetConfig(const QString &serverAddress);
    void doUpdateConfig(const QString &serverAddress, const QString &host, int port,
                        int width, int height, int framerate);
    void doUpdateHost(const QString &serverAddress, const QString &host);
    void doSwapResolution(const QString &serverAddress, int swap);

signals:
    // Health check result
    void healthCheckFinished(bool success, const QString &status);

    // Get config result
    void getConfigFinished(bool success, const QString &host, int port,
                           const QString &cameraName, const QString &encoderType,
                           int width, int height, int framerate);

    // Update config result
    void updateConfigFinished(bool success, const QString &message,
                              const QString &host, int port, int width, int height, int framerate);

    // Update host result
    void updateHostFinished(bool success, const QString &message);

    // Swap resolution result
    void swapResolutionFinished(bool success, const QString &message, int width, int height);

private:
    std::shared_ptr<grpc::Channel> createChannel(const QString &serverAddress);
};

class GrpcManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString serverAddress READ serverAddress WRITE setServerAddress NOTIFY serverAddressChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    // Config properties from gRPC
    Q_PROPERTY(QString txHost READ txHost NOTIFY configChanged)
    Q_PROPERTY(int txPort READ txPort NOTIFY configChanged)
    Q_PROPERTY(QString cameraName READ cameraName NOTIFY configChanged)
    Q_PROPERTY(QString encoderType READ encoderType NOTIFY configChanged)
    Q_PROPERTY(int txWidth READ txWidth NOTIFY configChanged)
    Q_PROPERTY(int txHeight READ txHeight NOTIFY configChanged)
    Q_PROPERTY(int txFramerate READ txFramerate NOTIFY configChanged)

public:
    explicit GrpcManager(QObject *parent = nullptr);
    ~GrpcManager();

    QString serverAddress() const { return m_serverAddress; }
    void setServerAddress(const QString &address);

    bool isConnected() const { return m_isConnected; }
    bool isBusy() const { return m_isBusy; }
    QString statusMessage() const { return m_statusMessage; }

    // Config getters
    QString txHost() const { return m_txHost; }
    int txPort() const { return m_txPort; }
    QString cameraName() const { return m_cameraName; }
    QString encoderType() const { return m_encoderType; }
    int txWidth() const { return m_txWidth; }
    int txHeight() const { return m_txHeight; }
    int txFramerate() const { return m_txFramerate; }

    // Actions (called from QML or ConfigManager)
    Q_INVOKABLE void healthCheck();
    Q_INVOKABLE void getConfig();
    Q_INVOKABLE void updateConfig(const QString &host, int port, int width, int height, int framerate);
    Q_INVOKABLE void updateHost(const QString &host);
    Q_INVOKABLE void swapResolution(int swap);

signals:
    void serverAddressChanged();
    void isConnectedChanged();
    void isBusyChanged();
    void statusMessageChanged();
    void configChanged();

    // Results signals for external listeners
    void healthCheckResult(bool success);
    void getConfigResult(bool success);
    void updateConfigResult(bool success, const QString &message);
    void updateHostResult(bool success, const QString &message);
    void swapResolutionResult(bool success, const QString &message, int width, int height);

    // Internal signals to trigger worker
    void startHealthCheck(const QString &serverAddress);
    void startGetConfig(const QString &serverAddress);
    void startUpdateConfig(const QString &serverAddress, const QString &host, int port,
                           int width, int height, int framerate);
    void startUpdateHost(const QString &serverAddress, const QString &host);
    void startSwapResolution(const QString &serverAddress, int swap);

private slots:
    void onHealthCheckFinished(bool success, const QString &status);
    void onGetConfigFinished(bool success, const QString &host, int port,
                             const QString &cameraName, const QString &encoderType,
                             int width, int height, int framerate);
    void onUpdateConfigFinished(bool success, const QString &message,
                                const QString &host, int port, int width, int height, int framerate);
    void onUpdateHostFinished(bool success, const QString &message);
    void onSwapResolutionFinished(bool success, const QString &message, int width, int height);

private:
    void setIsBusy(bool busy);
    void setStatusMessage(const QString &msg);

    QString m_serverAddress;
    bool m_isConnected = false;
    bool m_isBusy = false;
    QString m_statusMessage = "Ready";

    // Cached config from TX
    QString m_txHost;
    int m_txPort = 8888;
    QString m_cameraName;
    QString m_encoderType;
    int m_txWidth = 1280;
    int m_txHeight = 720;
    int m_txFramerate = 30;

    // Worker thread
    QThread *m_workerThread = nullptr;
    GrpcWorker *m_worker = nullptr;
};

#endif // GRPCMANAGER_H
