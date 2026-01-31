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

// ============ WifiWorker Implementation ============

QByteArray WifiWorker::sendSerialCommand(const QString &portName, const QByteArray &command, int timeoutMs)
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

    // Set timeouts
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = timeoutMs;
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
    Q_UNUSED(timeoutMs);
    return QByteArray();
#endif
}

int WifiWorker::parseResponseCode(const QByteArray &response)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return -1;
    }

    QJsonObject root = doc.object();
    return root["status"].toInt(-1);
}

void WifiWorker::doCheckStatus(const QString &serialPort)
{
    if (serialPort.isEmpty()) {
        LogManager::log("Status check failed: No camera connected");
        emit statusCheckFinished(false, "No camera connected");
        return;
    }

    LogManager::log(QString("Checking program status via %1...").arg(serialPort));

    // Wait for TX to send status 1 (ready) - just read without sending
    // The TX application sends {"status": 1, "payload": null} when ready
    QByteArray response = sendSerialCommand(serialPort, QByteArray(), 2000);

    if (response.isEmpty()) {
        LogManager::log("Status check failed: No response from camera");
        emit statusCheckFinished(false, "No response from camera");
        return;
    }

    LogManager::log(QString("Status response: %1").arg(QString::fromUtf8(response).trimmed()));

    int status = parseResponseCode(response);
    if (status == WifiProtocol::TX_READY) {
        LogManager::log("Camera ready (status 1 received)");
        emit statusCheckFinished(true, QString());
    } else {
        LogManager::log(QString("Camera status check failed: unexpected status %1").arg(status));
        emit statusCheckFinished(false, "Camera not ready");
    }
}

void WifiWorker::doScan(const QString &serialPort)
{
    QVariantList networks;
    QString connectedNetwork;

    if (serialPort.isEmpty()) {
        LogManager::log("WiFi scan failed: No camera connected");
        emit scanFinished(networks, connectedNetwork, false, "No camera connected");
        return;
    }

    LogManager::log(QString("Sending WiFi scan request (status 21) via %1...").arg(serialPort));

    // Send status 21 to request WiFi network list
    // Format: {"status": 21, "payload": null}
    QByteArray command = "{\"status\":21,\"payload\":null}\n";
    QByteArray response = sendSerialCommand(serialPort, command, 10000); // Longer timeout for WiFi scan

    if (response.isEmpty()) {
        LogManager::log("WiFi scan failed: No response from camera");
        emit scanFinished(networks, connectedNetwork, false, "No response from camera");
        return;
    }

    LogManager::log(QString("WiFi scan response: %1").arg(QString::fromUtf8(response).trimmed()));

    // Parse JSON response
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        LogManager::log(QString("JSON parse error: %1").arg(parseError.errorString()));
        emit scanFinished(networks, connectedNetwork, false, "Invalid response from camera");
        return;
    }

    QJsonObject root = doc.object();
    int status = root["status"].toInt(-1);

    // Expect status 4 for WiFi list response
    if (status != WifiProtocol::TX_WIFI_LIST) {
        LogManager::log(QString("Unexpected response status: %1 (expected 4)").arg(status));
        emit scanFinished(networks, connectedNetwork, false, "Unexpected response from camera");
        return;
    }

    // Parse payload - can be either a JSON array or a JSON string containing an array
    QJsonArray wifiArray;
    QJsonValue payloadValue = root["payload"];

    if (payloadValue.isArray()) {
        // Payload is directly an array
        wifiArray = payloadValue.toArray();
    } else if (payloadValue.isString()) {
        // Payload is a JSON string that needs to be parsed
        QString payloadStr = payloadValue.toString();
        QJsonDocument payloadDoc = QJsonDocument::fromJson(payloadStr.toUtf8());
        if (payloadDoc.isArray()) {
            wifiArray = payloadDoc.array();
        } else {
            LogManager::log("Payload string is not a valid JSON array");
        }
    } else {
        LogManager::log("Unexpected payload type in WiFi list response");
    }

    for (const QJsonValue &value : wifiArray) {
        QJsonObject wifiObj = value.toObject();

        QString ssid = wifiObj["SSID"].toString();
        QString bssid = wifiObj["BSSID"].toString();

        if (ssid.isEmpty() || bssid.isEmpty()) {
            continue;
        }

        QVariantMap network;
        network["ssid"] = ssid;
        network["bssid"] = bssid;
        network["isSecured"] = true; // Assume secured by default (not provided in protocol)
        network["isConnected"] = false; // Will be updated if connected network info is available

        // Parse signal strength if available
        if (wifiObj.contains("signal_dbm")) {
            int rssi = wifiObj["signal_dbm"].toInt(-70);
            int signalStrength = qBound(0, (rssi + 100) * 2, 100);
            network["signalStrength"] = signalStrength;
        } else {
            network["signalStrength"] = 50; // Default signal strength
        }

        networks.append(network);
    }

    LogManager::log(QString("WiFi scan complete (status 4): Found %1 networks").arg(networks.size()));
    emit scanFinished(networks, connectedNetwork, true, QString());
}

void WifiWorker::doConnect(const QString &serialPort, const QString &bssid, const QString &password, const QString &rxIp)
{
    if (serialPort.isEmpty()) {
        LogManager::log("WiFi connect failed: No camera connected");
        emit connectFinished(false, QString(), "No camera connected");
        return;
    }

    LogManager::log(QString("Connecting to WiFi network (BSSID: %1)...").arg(bssid));

    // Step 1: Send status 22 with BSSID and password
    // Format: {"status": 22, "payload": {"BSSID": "bssid", "pass": "password"}}
    QJsonObject payload;
    payload["BSSID"] = bssid;
    payload["pass"] = password;

    QJsonObject connectCmd;
    connectCmd["status"] = WifiProtocol::RX_CONNECT;
    connectCmd["payload"] = payload;

    QByteArray command = QJsonDocument(connectCmd).toJson(QJsonDocument::Compact) + "\n";
    LogManager::log(QString("Sending connect request (status 22) for BSSID: %1").arg(bssid));

    QByteArray response = sendSerialCommand(serialPort, command, 30000); // 30s timeout for WiFi connection

    if (response.isEmpty()) {
        LogManager::log("WiFi connect failed: No response from camera");
        emit connectFinished(false, QString(), "No response from camera");
        return;
    }

    LogManager::log(QString("Connect response: %1").arg(QString::fromUtf8(response).trimmed()));

    // Parse response
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        LogManager::log(QString("JSON parse error: %1").arg(parseError.errorString()));
        emit connectFinished(false, QString(), "Invalid response from camera");
        return;
    }

    QJsonObject root = doc.object();
    int status = root["status"].toInt(-1);

    // Check for status 2 (success) or status 3 (failure)
    if (status == WifiProtocol::TX_WIFI_FAILED) {
        LogManager::log("WiFi connect error (status 3): Connection failed");
        emit connectFinished(false, QString(), "WiFi connection failed");
        return;
    }

    if (status != WifiProtocol::TX_WIFI_SUCCESS) {
        LogManager::log(QString("Unexpected response status: %1 (expected 2)").arg(status));
        emit connectFinished(false, QString(), "Unexpected response from camera");
        return;
    }

    // Status 2: Connection successful, extract TX IP from payload
    // Format: {"status": 2, "payload": {"IPAddr": "192.168.x.x"}}
    // Note: payload may be a JSON object or a JSON string that needs parsing
    QJsonValue payloadValue = root["payload"];
    QJsonObject responsePayload;

    if (payloadValue.isObject()) {
        responsePayload = payloadValue.toObject();
    } else if (payloadValue.isString()) {
        // Payload is a JSON string that needs to be parsed
        QString payloadStr = payloadValue.toString();
        QJsonDocument payloadDoc = QJsonDocument::fromJson(payloadStr.toUtf8());
        if (payloadDoc.isObject()) {
            responsePayload = payloadDoc.object();
        } else {
            LogManager::log("Payload string is not a valid JSON object");
        }
    }

    QString txIp = responsePayload["IPAddr"].toString();

    if (txIp.isEmpty()) {
        LogManager::log("TX IP address is empty in response");
        emit connectFinished(false, QString(), "TX IP address not provided");
        return;
    }

    LogManager::log(QString("WiFi connection successful (status 2), TX IP: %1").arg(txIp));

    // Step 2: Send RX's IP address with status 23
    // Format: {"status": 23, "payload": {"IPAddr": "192.168.x.x"}}
    QJsonObject ipPayload;
    ipPayload["IPAddr"] = rxIp;

    QJsonObject ipCmd;
    ipCmd["status"] = WifiProtocol::RX_SEND_IP;
    ipCmd["payload"] = ipPayload;

    command = QJsonDocument(ipCmd).toJson(QJsonDocument::Compact) + "\n";
    LogManager::log(QString("Sending RX IP address (status 23): %1").arg(rxIp));

    // Send RX IP - no response expected according to protocol
    sendSerialCommand(serialPort, command, 1000);

    LogManager::log("WiFi connection flow completed successfully!");
    emit connectFinished(true, txIp, QString());
}

// ============ WifiManager Implementation ============

WifiManager::WifiManager(QObject *parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_worker(new WifiWorker())
{
    // Move worker to background thread
    m_worker->moveToThread(m_workerThread);

    // Connect signals for status check
    connect(this, &WifiManager::startStatusCheck, m_worker, &WifiWorker::doCheckStatus);
    connect(m_worker, &WifiWorker::statusCheckFinished, this, &WifiManager::onStatusCheckFinished);

    // Connect signals for scan
    connect(this, &WifiManager::startScan, m_worker, &WifiWorker::doScan);
    connect(m_worker, &WifiWorker::scanFinished, this, &WifiManager::onScanFinished);

    // Connect signals for connect
    connect(this, &WifiManager::startConnect, m_worker, &WifiWorker::doConnect);
    connect(m_worker, &WifiWorker::connectFinished, this, &WifiManager::onConnectFinished);

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
        m_cameraReady = false;
        emit serialPortChanged();
        emit cameraReadyChanged();
    }
}

void WifiManager::checkStatus()
{
    if (m_serialPort.isEmpty()) {
        m_errorMessage = "No camera connected";
        emit errorMessageChanged();
        return;
    }

    emit startStatusCheck(m_serialPort);
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

void WifiManager::connectToNetwork(const QString &bssid, const QString &password, const QString &rxIp)
{
    if (m_connectInProgress) {
        return;
    }

    if (m_serialPort.isEmpty()) {
        m_errorMessage = "No camera connected";
        emit errorMessageChanged();
        emit connectionFailed(m_errorMessage);
        return;
    }

    m_connectInProgress = true;
    emit isConnectingChanged();
    emit startConnect(m_serialPort, bssid, password, rxIp);
}

void WifiManager::onStatusCheckFinished(bool ready, const QString &errorMessage)
{
    if (m_cameraReady != ready) {
        m_cameraReady = ready;
        emit cameraReadyChanged();
    }

    if (!ready) {
        m_errorMessage = errorMessage;
        emit errorMessageChanged();
    } else {
        m_errorMessage.clear();
        emit errorMessageChanged();
    }
}

void WifiManager::onScanFinished(const QVariantList &networks, const QString &connectedNetwork, bool success, const QString &errorMessage)
{
    m_scanInProgress = false;
    emit isScanningChanged();

    if (success) {
        if (m_networks != networks) {
            m_networks = networks;
            emit networksChanged();
        }
        if (m_connectedNetwork != connectedNetwork) {
            m_connectedNetwork = connectedNetwork;
            emit connectedNetworkChanged();
        }
        m_errorMessage.clear();
    } else {
        LogManager::log(QString("WiFi scan error: %1").arg(errorMessage));
        m_errorMessage = errorMessage;
    }
    emit errorMessageChanged();
}

void WifiManager::onConnectFinished(bool success, const QString &txIp, const QString &errorMessage)
{
    m_connectInProgress = false;
    emit isConnectingChanged();

    if (success) {
        m_txIpAddress = txIp;
        emit txIpAddressChanged();
        m_errorMessage.clear();
        emit errorMessageChanged();
        emit connectionSucceeded(txIp);
        LogManager::log(QString("WiFi connection complete. TX IP: %1").arg(txIp));
    } else {
        m_errorMessage = errorMessage;
        emit errorMessageChanged();
        emit connectionFailed(errorMessage);
        LogManager::log(QString("WiFi connection failed: %1").arg(errorMessage));
    }
}
