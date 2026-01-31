#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QSettings>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
    
    // Actions
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void loadConfigFromCamera();
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE QString detectLocalIp();
    Q_INVOKABLE void setDirectionSaved(bool saved);

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

private:
    void setStatusMessage(const QString &msg);
    void setIsBusy(bool busy);
    void applyResolutionSelection();
    void applyFramerateSelection();
    bool rotateIsHorizontal(int rotate);
    
    // HTTP operations
    bool httpTestConnection();
    bool httpSendConfig();
    bool httpSendRxRotate(int rotate);
    
    // Serial operations
    QByteArray sendSerialCommand(const QString &command);
    void fetchConfigFromSerial();
    bool sendRotateViaSerial();
    
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
    
    // Settings storage
    QSettings *m_settings = nullptr;
};

#endif // CONFIGMANAGER_H
