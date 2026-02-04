#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QSettings>
#include <QThread>

// Forward declaration
class ConfigWorker;

// Worker class that runs on a background thread for I/O operations
class ConfigWorker : public QObject
{
    Q_OBJECT
public:
    explicit ConfigWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void doTestConnection(const QString &host, int port);
    void doSaveConfig(const QString &serialPort, const QString &txServerIp, int txHttpPort,
                      const QString &rxHostIp, int rxStreamPort,
                      int width, int height, int framerate, int rotate);
    void doLoadConfigFromCamera(const QString &serialPort);

signals:
    void testConnectionFinished(bool success);
    void saveConfigFinished(bool success, const QString &statusMessage);
    void loadConfigFinished(bool success, const QString &rxHostIp, int resolutionIndex, int framerateIndex);

private:
    bool httpTestConnection(const QString &host, int port);
    bool httpSendConfig(const QString &txServerIp, int txHttpPort,
                        const QString &rxHostIp, int rxStreamPort,
                        int width, int height, int framerate);
    QByteArray sendSerialCommand(const QString &serialPort, const QString &command);
    bool sendRotateViaSerial(const QString &serialPort, int rotate);
};

class ConfigManager : public QObject
{
    Q_OBJECT

    // IP addresses
    Q_PROPERTY(QString txServerIp READ txServerIp WRITE setTxServerIp NOTIFY txServerIpChanged)
    Q_PROPERTY(QString rxHostIp READ rxHostIp WRITE setRxHostIp NOTIFY rxHostIpChanged)

    // Resolution
    Q_PROPERTY(int resolutionIndex READ resolutionIndex WRITE setResolutionIndex NOTIFY resolutionIndexChanged)
    Q_PROPERTY(QStringList resolutionOptions READ resolutionOptions CONSTANT)
    Q_PROPERTY(int width READ width NOTIFY resolutionChanged)
    Q_PROPERTY(int height READ height NOTIFY resolutionChanged)

    // Framerate
    Q_PROPERTY(int framerateIndex READ framerateIndex WRITE setFramerateIndex NOTIFY framerateIndexChanged)
    Q_PROPERTY(QStringList framerateOptions READ framerateOptions CONSTANT)
    Q_PROPERTY(int framerate READ framerate NOTIFY framerateChanged)

    // Rotate
    Q_PROPERTY(int rotate READ rotate WRITE setRotate NOTIFY rotateChanged)

    // Direction saved flag (set when Save button is pressed in camera direction)
    Q_PROPERTY(bool directionSaved READ directionSaved NOTIFY directionSavedChanged)

    // Camera connected flag (set when testConnection succeeds)
    Q_PROPERTY(bool cameraConnected READ cameraConnected NOTIFY cameraConnectedChanged)

    // Status
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)

    // Serial port (from SerialPortManager)
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort NOTIFY serialPortChanged)

    // gRPC server address (for direct TX communication)
    Q_PROPERTY(QString grpcServerAddress READ grpcServerAddress WRITE setGrpcServerAddress NOTIFY grpcServerAddressChanged)
    Q_PROPERTY(bool useGrpc READ useGrpc WRITE setUseGrpc NOTIFY useGrpcChanged)

public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();

    // IP getters/setters
    QString txServerIp() const { return m_txServerIp; }
    void setTxServerIp(const QString &ip);

    QString rxHostIp() const { return m_rxHostIp; }
    void setRxHostIp(const QString &ip);

    // Resolution getters/setters
    int resolutionIndex() const { return m_resolutionIndex; }
    void setResolutionIndex(int index);
    QStringList resolutionOptions() const { return m_resolutionOptions; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    // Framerate getters/setters
    int framerateIndex() const { return m_framerateIndex; }
    void setFramerateIndex(int index);
    QStringList framerateOptions() const { return m_framerateOptions; }
    int framerate() const { return m_framerate; }

    // Rotate getter/setter
    int rotate() const { return m_rotate; }
    void setRotate(int rotate);

    // Direction saved getter
    bool directionSaved() const { return m_directionSaved; }

    // Camera connected getter
    bool cameraConnected() const { return m_cameraConnected; }

    // Status
    QString statusMessage() const { return m_statusMessage; }
    bool isBusy() const { return m_isBusy; }

    // Serial port
    QString serialPort() const { return m_serialPort; }
    void setSerialPort(const QString &port);

    // gRPC server address
    QString grpcServerAddress() const { return m_grpcServerAddress; }
    void setGrpcServerAddress(const QString &address);
    bool useGrpc() const { return m_useGrpc; }
    void setUseGrpc(bool use);

    // Actions
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void loadConfigFromCamera();
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE QString detectLocalIp();
    Q_INVOKABLE void setDirectionSaved(bool saved);
    Q_INVOKABLE QString detectLocalIpForTarget(const QString &targetIp);

signals:
    void txServerIpChanged();
    void rxHostIpChanged();
    void resolutionIndexChanged();
    void resolutionChanged();
    void framerateIndexChanged();
    void framerateChanged();
    void rotateChanged();
    void directionSavedChanged();
    void cameraConnectedChanged();
    void statusMessageChanged();
    void isBusyChanged();
    void serialPortChanged();
    void configSaved();
    void connectionTestResult(bool success);
    void grpcServerAddressChanged();
    void useGrpcChanged();

    // Internal signals to trigger worker
    void startTestConnection(const QString &host, int port);
    void startSaveConfig(const QString &serialPort, const QString &txServerIp, int txHttpPort,
                         const QString &rxHostIp, int rxStreamPort,
                         int width, int height, int framerate, int rotate);
    void startLoadConfig(const QString &serialPort);

private slots:
    void onTestConnectionFinished(bool success);
    void onSaveConfigFinished(bool success, const QString &statusMessage);
    void onLoadConfigFinished(bool success, const QString &rxHostIp, int resolutionIndex, int framerateIndex);

private:
    void setStatusMessage(const QString &msg);
    void setIsBusy(bool busy);
    void applyResolutionSelection();
    void applyFramerateSelection();
    bool rotateIsHorizontal(int rotate);

    // Config values
    QString m_txServerIp = "192.168.4.1";  // Default TX IP
    QString m_rxHostIp = "127.0.0.1";      // Default RX IP (localhost)
    int m_txHttpPort = 80;                  // TX server HTTP port
    int m_rxStreamPort = 8888;              // RX stream port

    // Resolution presets
    QStringList m_resolutionOptions = {"1280 x 720", "720 x 1280"};
    int m_resolutionIndex = 0;
    int m_width = 1280;
    int m_height = 720;

    // Framerate presets
    QStringList m_framerateOptions = {"30 fps", "50 fps", "60 fps"};
    QList<int> m_framerateValues = {30, 50, 60};
    int m_framerateIndex = 0;
    int m_framerate = 30;

    // Rotate (0-3)
    int m_rotate = 0;

    // Direction saved flag
    bool m_directionSaved = false;

    // Camera connected flag
    bool m_cameraConnected = false;

    // Status
    QString m_statusMessage = "Ready";
    bool m_isBusy = false;

    // Serial port for camera communication
    QString m_serialPort;

    // gRPC settings
    QString m_grpcServerAddress;
    bool m_useGrpc = true;  // Default to using gRPC

    // Settings storage
    QSettings *m_settings = nullptr;

    // Worker thread
    QThread *m_workerThread = nullptr;
    ConfigWorker *m_worker = nullptr;
};

#endif // CONFIGMANAGER_H
