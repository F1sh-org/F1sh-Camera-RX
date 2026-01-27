#include "wifimanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#ifdef _WIN32
#include <windows.h>
#endif

// JSON command to request WiFi scan from camera
static const QByteArray kWifiScanCommand = "{\"wifi_scan\":1}\n";

// ============ WifiWorker Implementation ============

QByteArray WifiWorker::sendSerialCommand(const QString &portName, const QByteArray &command)
{
#ifdef _WIN32
    QString devicePath;
    if (portName.startsWith("COM")) {
        int num = portName.mid(3).toInt();
        if (num >= 10) {
            devicePath = QString("\\\\.\\%1").arg(portName);
        } else {
            devicePath = portName;
        }
    } else {
        devicePath = portName;
    }
    
    HANDLE h = CreateFileA(devicePath.toLatin1().constData(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        qWarning() << "Failed to open serial port:" << portName;
        return QByteArray();
    }
    
    // Configure serial port
    DCB dcb;
    memset(&dcb, 0, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return QByteArray();
    }
    
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    
    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return QByteArray();
    }
    
    // Set timeouts - longer timeout for WiFi scan (can take a few seconds)
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 5000; // 5 second timeout for WiFi scan
    timeouts.WriteTotalTimeoutConstant = 200;
    SetCommTimeouts(h, &timeouts);
    
    // Send command
    DWORD written = 0;
    WriteFile(h, command.constData(), command.size(), &written, NULL);
    
    // Read response
    QByteArray response;
    char readbuf[4096] = {0};
    DWORD bytesRead = 0;
    
    // Read until we get a complete JSON response or timeout
    while (ReadFile(h, readbuf, sizeof(readbuf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        response.append(readbuf, bytesRead);
        
        // Check if we have a complete JSON object
        if (response.contains('}')) {
            break;
        }
    }
    
    CloseHandle(h);
    return response;
#else
    Q_UNUSED(portName);
    Q_UNUSED(command);
    return QByteArray();
#endif
}

void WifiWorker::doScan(const QString &serialPort)
{
    QVariantList networks;
    
    if (serialPort.isEmpty()) {
        LogManager::log("WiFi scan failed: No camera connected");
        emit scanFinished(networks, false, "No camera connected");
        return;
    }
    
    LogManager::log(QString("Starting WiFi scan via %1...").arg(serialPort));
    qDebug() << "Scanning WiFi via serial port:" << serialPort;
    
    // Send WiFi scan command to camera
    QByteArray response = sendSerialCommand(serialPort, kWifiScanCommand);
    
    if (response.isEmpty()) {
        LogManager::log("WiFi scan failed: No response from camera");
        emit scanFinished(networks, false, "No response from camera");
        return;
    }
    
    qDebug() << "WiFi scan response:" << response;
    
    // Parse JSON response
    // Expected format: {"wifi_list":[{"ssid":"NetworkName","rssi":-50,"secure":true},...]}}
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        emit scanFinished(networks, false, "Invalid response from camera");
        return;
    }
    
    QJsonObject root = doc.object();
    
    // Check for error response
    if (root.contains("error")) {
        emit scanFinished(networks, false, root["error"].toString());
        return;
    }
    
    // Parse wifi_list array
    QJsonArray wifiArray = root["wifi_list"].toArray();
    
    for (const QJsonValue &value : wifiArray) {
        QJsonObject wifiObj = value.toObject();
        
        QString ssid = wifiObj["ssid"].toString();
        if (ssid.isEmpty()) {
            continue;
        }
        
        QVariantMap network;
        network["ssid"] = ssid;
        
        // Convert RSSI to signal strength percentage (typical range: -100 to -30 dBm)
        int rssi = wifiObj["rssi"].toInt(-70);
        int signalStrength = qBound(0, (rssi + 100) * 2, 100);
        network["signalStrength"] = signalStrength;
        
        network["isSecured"] = wifiObj["secure"].toBool(true);
        network["isConnected"] = wifiObj["connected"].toBool(false);
        
        networks.append(network);
    }
    
    // Sort by signal strength (connected first, then by signal)
    std::sort(networks.begin(), networks.end(), [](const QVariant &a, const QVariant &b) {
        QVariantMap mapA = a.toMap();
        QVariantMap mapB = b.toMap();
        
        // Connected networks first
        if (mapA["isConnected"].toBool() != mapB["isConnected"].toBool()) {
            return mapA["isConnected"].toBool();
        }
        
        // Then by signal strength
        return mapA["signalStrength"].toInt() > mapB["signalStrength"].toInt();
    });
    
    emit scanFinished(networks, true, QString());
}

// ============ WifiManager Implementation ============

WifiManager::WifiManager(QObject *parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_worker(new WifiWorker())
{
    // Move worker to background thread
    m_worker->moveToThread(m_workerThread);
    
    // Connect signals
    connect(this, &WifiManager::startScan, m_worker, &WifiWorker::doScan);
    connect(m_worker, &WifiWorker::scanFinished, this, &WifiManager::onScanFinished);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    
    // Start worker thread
    m_workerThread->start();
}

WifiManager::~WifiManager()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

void WifiManager::setSerialPort(const QString &port)
{
    if (m_serialPort != port) {
        m_serialPort = port;
        emit serialPortChanged();
    }
}

void WifiManager::refresh()
{
    // Don't start new scan if one is already in progress
    if (m_scanInProgress) {
        return;
    }
    
    if (m_serialPort.isEmpty()) {
        m_errorMessage = "No camera connected";
        emit errorMessageChanged();
        return;
    }
    
    m_scanInProgress = true;
    emit isScanningChanged();
    emit startScan(m_serialPort);
}

void WifiManager::onScanFinished(const QVariantList &networks, bool success, const QString &errorMessage)
{
    m_scanInProgress = false;
    emit isScanningChanged();
    
    if (success) {
        if (m_networks != networks) {
            m_networks = networks;
            emit networksChanged();
        }
        LogManager::log(QString("WiFi scan complete: Found %1 networks").arg(networks.size()));
        m_errorMessage.clear();
    } else {
        LogManager::log(QString("WiFi scan error: %1").arg(errorMessage));
        m_errorMessage = errorMessage;
    }
    emit errorMessageChanged();
}
