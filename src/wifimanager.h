#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QThread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Protocol status codes as defined in the serial communication standard
// Message format: {"status": [int], "payload": [data]}\n
namespace WifiProtocol {
    // TX Application codes (received from camera)
    constexpr int TX_READY = 1;              // Program started / Ready
    constexpr int TX_WIFI_SUCCESS = 2;       // WiFi connection successful (payload: IPAddr)
    constexpr int TX_WIFI_FAILED = 3;        // WiFi connection failed
    constexpr int TX_WIFI_LIST = 4;          // WiFi network list (payload: [{SSID, BSSID}, ...])
    constexpr int TX_CONFIG = 5;             // Current configuration

    // RX Application codes (sent to camera)
    constexpr int RX_SCAN_REQUEST = 21;      // Request WiFi network list
    constexpr int RX_CONNECT = 22;           // Request WiFi connection (payload: BSSID, pass)
    constexpr int RX_SEND_IP = 23;           // Send RX IP address (payload: IPAddr)
    constexpr int RX_SWAP_RESOLUTION = 24;   // Change resolution for rotation (payload: swap)
}

// Worker class that runs on a background thread
class WifiWorker : public QObject
{
    Q_OBJECT
public:
    explicit WifiWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void doCheckStatus(const QString &serialPort);
    void doScan(const QString &serialPort);
    void doConnect(const QString &serialPort, const QString &bssid, const QString &password, const QString &rxIp);

signals:
    void statusCheckFinished(bool ready, const QString &errorMessage);
    void scanFinished(const QVariantList &networks, const QString &connectedNetwork, bool success, const QString &errorMessage);
    void connectFinished(bool success, const QString &txIp, const QString &errorMessage);

private:
    QByteArray sendSerialCommand(const QString &portName, const QByteArray &command, int timeoutMs = 5000);
    int parseResponseCode(const QByteArray &response);
};

class WifiManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(bool isConnecting READ isConnecting NOTIFY isConnectingChanged)
    Q_PROPERTY(bool cameraReady READ cameraReady NOTIFY cameraReadyChanged)
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort NOTIFY serialPortChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString connectedNetwork READ connectedNetwork NOTIFY connectedNetworkChanged)
    Q_PROPERTY(QString txIpAddress READ txIpAddress NOTIFY txIpAddressChanged)

public:
    explicit WifiManager(QObject *parent = nullptr);
    ~WifiManager();

    QVariantList networks() const { return m_networks; }
    bool isScanning() const { return m_scanInProgress; }
    bool isConnecting() const { return m_connectInProgress; }
    bool cameraReady() const { return m_cameraReady; }
    QString serialPort() const { return m_serialPort; }
    QString errorMessage() const { return m_errorMessage; }
    QString connectedNetwork() const { return m_connectedNetwork; }
    QString txIpAddress() const { return m_txIpAddress; }

    void setSerialPort(const QString &port);

    Q_INVOKABLE void checkStatus();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void connectToNetwork(const QString &bssid, const QString &password, const QString &rxIp);

signals:
    void networksChanged();
    void isScanningChanged();
    void isConnectingChanged();
    void cameraReadyChanged();
    void serialPortChanged();
    void errorMessageChanged();
    void connectedNetworkChanged();
    void txIpAddressChanged();
    void connectionSucceeded(const QString &txIp);
    void connectionFailed(const QString &error);

    // Internal signals to trigger worker
    void startStatusCheck(const QString &serialPort);
    void startScan(const QString &serialPort);
    void startConnect(const QString &serialPort, const QString &bssid, const QString &password, const QString &rxIp);

private slots:
    void onStatusCheckFinished(bool ready, const QString &errorMessage);
    void onScanFinished(const QVariantList &networks, const QString &connectedNetwork, bool success, const QString &errorMessage);
    void onConnectFinished(bool success, const QString &txIp, const QString &errorMessage);

private:
    QVariantList m_networks;
    QString m_serialPort;
    QString m_errorMessage;
    QString m_connectedNetwork;
    QString m_txIpAddress;
    bool m_scanInProgress = false;
    bool m_connectInProgress = false;
    bool m_cameraReady = false;

    QThread *m_workerThread = nullptr;
    WifiWorker *m_worker = nullptr;
};

#endif // WIFIMANAGER_H
