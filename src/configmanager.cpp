#include "configmanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QSerialPort>

// Resolution presets (width, height)
static const int kResolutionPresets[][2] = {
    { 1280, 720 },
    { 720, 1280 }
};

// ============ ConfigWorker Implementation ============

// Cross-platform HTTP request using Qt's QNetworkAccessManager
static bool doHttpRequest(const QString &host, int port, const QString &path,
                          const QString &method, const QByteArray &body,
                          QByteArray &response)
{
    response.clear();

    QNetworkAccessManager manager;
    QUrl url;
    url.setScheme("http");
    url.setHost(host);
    url.setPort(port);
    url.setPath(path);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(5000); // 5 second timeout

    QNetworkReply *reply = nullptr;

    if (method == "GET") {
        reply = manager.get(request);
    } else if (method == "POST") {
        reply = manager.post(request, body);
    } else {
        qWarning() << "Unsupported HTTP method:" << method;
        return false;
    }

    // Wait for the reply synchronously using event loop (OK since we're on worker thread)
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    bool success = false;

    if (reply->error() == QNetworkReply::NoError) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response = reply->readAll();

        if (statusCode != 200) {
            qWarning() << "HTTP status code:" << statusCode;
        }

        success = (statusCode == 200);
    } else {
        qWarning() << "HTTP request failed:" << reply->errorString();
    }

    reply->deleteLater();
    return success;
}

bool ConfigWorker::httpTestConnection(const QString &host, int port)
{
    QByteArray response;
    bool success = doHttpRequest(host, port, "/health", "GET", QByteArray(), response);

    if (success && !response.isEmpty()) {
        qDebug() << "Health check response:" << response;

        // Parse JSON response
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(response, &error);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString status = obj["status"].toString();
            if (status == "healthy") {
                return true;
            }
        }
    }

    return false;
}

bool ConfigWorker::httpSendConfig(const QString &txServerIp, int txHttpPort,
                                   const QString &rxHostIp, int rxStreamPort,
                                   int width, int height, int framerate)
{
    // Create JSON config
    QJsonObject config;
    config["host"] = rxHostIp;
    config["port"] = rxStreamPort;
    config["width"] = width;
    config["height"] = height;
    config["framerate"] = framerate;

    QJsonDocument doc(config);
    QByteArray body = doc.toJson(QJsonDocument::Compact);

    qDebug() << "Sending config:" << body;

    QByteArray response;
    bool success = doHttpRequest(txServerIp, txHttpPort, "/config", "POST", body, response);

    if (success) {
        qDebug() << "Config response:" << response;
    }

    return success;
}

QByteArray ConfigWorker::sendSerialCommand(const QString &serialPort, const QString &command)
{
    if (serialPort.isEmpty()) {
        return QByteArray();
    }

    QSerialPort serial;
    serial.setPortName(serialPort);
    serial.setBaudRate(QSerialPort::Baud115200);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::ReadWrite)) {
        qWarning() << "Failed to open serial port:" << serialPort << serial.errorString();
        return QByteArray();
    }

    // Send command
    QByteArray cmdBytes = command.toUtf8();
    serial.write(cmdBytes);
    serial.flush();

    // Read response
    QByteArray response;

    // Wait for data with timeout
    if (serial.waitForReadyRead(4000)) {
        response = serial.readAll();

        // Continue reading if more data is available
        while (serial.waitForReadyRead(100)) {
            response.append(serial.readAll());

            // Check if we have a complete JSON object
            if (response.contains('}')) {
                break;
            }
        }
    }

    serial.close();
    return response;
}

bool ConfigWorker::sendRotateViaSerial(const QString &serialPort, int rotate)
{
    if (serialPort.isEmpty()) {
        LogManager::log("Cannot send rotation: No serial port connected");
        return false;
    }

    // Send status 24 with swap value
    // swap = 1 if rotate is 1 or 3 (portrait mode), 0 otherwise
    int swap = (rotate == 1 || rotate == 3) ? 1 : 0;

    QString command = QString("{\"status\":24,\"payload\":{\"swap\":%1}}\n").arg(swap);
    LogManager::log(QString("Sending rotation via serial (status 24, swap=%1)").arg(swap));

    QByteArray response = sendSerialCommand(serialPort, command);

    if (response.isEmpty()) {
        LogManager::log("No response to rotation command (this may be OK)");
        // No response expected for status 24 according to protocol
        return true;
    }

    LogManager::log(QString("Rotation command response: %1").arg(QString::fromUtf8(response).trimmed()));
    return true;
}

void ConfigWorker::doTestConnection(const QString &host, int port)
{
    LogManager::log(QString("Testing connection to TX server at %1...").arg(host));

    bool success = httpTestConnection(host, port);

    if (success) {
        LogManager::log("TX server connection successful");
    } else {
        LogManager::log("TX server connection failed");
    }

    emit testConnectionFinished(success);
}

void ConfigWorker::doSaveConfig(const QString &serialPort, const QString &txServerIp, int txHttpPort,
                                 const QString &rxHostIp, int rxStreamPort,
                                 int width, int height, int framerate, int rotate)
{
    LogManager::log("Saving configuration...");
    LogManager::log(QString("Config: %1x%2 @ %3fps, rotate=%4").arg(width).arg(height).arg(framerate).arg(rotate));

    QString statusMessage;

    // If we have a serial port, send rotation via serial protocol (status 24)
    if (!serialPort.isEmpty()) {
        if (!sendRotateViaSerial(serialPort, rotate)) {
            LogManager::log("Warning: Failed to send rotation via serial");
            // Continue anyway - save locally
        }
    }

    // If TX server IP is set and we're connected to WiFi, try HTTP config
    if (!txServerIp.isEmpty() && txServerIp != "192.168.4.1") {
        if (httpTestConnection(txServerIp, txHttpPort)) {
            if (httpSendConfig(txServerIp, txHttpPort, rxHostIp, rxStreamPort, width, height, framerate)) {
                LogManager::log("Configuration sent to TX server via HTTP");
            } else {
                LogManager::log("Warning: Failed to send config via HTTP");
            }
        } else {
            LogManager::log("TX server not reachable via HTTP (this is OK if using serial)");
        }
    }

    LogManager::log("Configuration saved successfully");
    emit saveConfigFinished(true, "Configuration saved");
}

void ConfigWorker::doLoadConfigFromCamera(const QString &serialPort)
{
    if (serialPort.isEmpty()) {
        emit loadConfigFinished(false, QString(), -1, -1);
        return;
    }

    // Send status 5 command to get TX config
    QByteArray response = sendSerialCommand(serialPort, "{\"status\":5}\n");

    if (response.isEmpty()) {
        emit loadConfigFinished(false, QString(), -1, -1);
        return;
    }

    qDebug() << "TX config response:" << response;

    // Parse JSON response
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &error);

    if (error.error != QJsonParseError::NoError) {
        emit loadConfigFinished(false, QString(), -1, -1);
        return;
    }

    QJsonObject root = doc.object();

    // Check for status 5 response
    if (root["status"].toInt() != 5) {
        emit loadConfigFinished(false, QString(), -1, -1);
        return;
    }

    // Parse payload - can be object or string
    QJsonObject configObj;
    if (root["payload"].isObject()) {
        configObj = root["payload"].toObject();
    } else if (root["payload"].isString()) {
        QJsonDocument payloadDoc = QJsonDocument::fromJson(root["payload"].toString().toUtf8());
        if (payloadDoc.isObject()) {
            configObj = payloadDoc.object();
        }
    }

    if (configObj.isEmpty()) {
        emit loadConfigFinished(false, QString(), -1, -1);
        return;
    }

    // Extract config values
    QString rxHostIp;
    int resolutionIndex = -1;
    int framerateIndex = -1;

    if (configObj.contains("host")) {
        rxHostIp = configObj["host"].toString();
    }

    if (configObj.contains("width") && configObj.contains("height")) {
        int w = configObj["width"].toInt();
        int h = configObj["height"].toInt();

        // Find matching resolution preset
        for (int i = 0; i < (int)(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0])); i++) {
            if ((kResolutionPresets[i][0] == w && kResolutionPresets[i][1] == h) ||
                (kResolutionPresets[i][0] == h && kResolutionPresets[i][1] == w)) {
                resolutionIndex = i;
                break;
            }
        }
    }

    if (configObj.contains("framerate")) {
        int fr = configObj["framerate"].toInt();
        static const int framerateValues[] = {30, 50, 60};
        for (int i = 0; i < 3; i++) {
            if (framerateValues[i] == fr) {
                framerateIndex = i;
                break;
            }
        }
    }

    emit loadConfigFinished(true, rxHostIp, resolutionIndex, framerateIndex);
}

// ============ ConfigManager Implementation ============

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings("F1sh", "CameraRX", this))
    , m_workerThread(new QThread(this))
    , m_worker(new ConfigWorker())
{
    // Move worker to background thread
    m_worker->moveToThread(m_workerThread);

    // Connect signals for test connection
    connect(this, &ConfigManager::startTestConnection, m_worker, &ConfigWorker::doTestConnection);
    connect(m_worker, &ConfigWorker::testConnectionFinished, this, &ConfigManager::onTestConnectionFinished);

    // Connect signals for save config
    connect(this, &ConfigManager::startSaveConfig, m_worker, &ConfigWorker::doSaveConfig);
    connect(m_worker, &ConfigWorker::saveConfigFinished, this, &ConfigManager::onSaveConfigFinished);

    // Connect signals for load config
    connect(this, &ConfigManager::startLoadConfig, m_worker, &ConfigWorker::doLoadConfigFromCamera);
    connect(m_worker, &ConfigWorker::loadConfigFinished, this, &ConfigManager::onLoadConfigFinished);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Start worker thread
    m_workerThread->start();

    // Load saved settings on startup
    loadSettings();
}

ConfigManager::~ConfigManager()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

void ConfigManager::setTxServerIp(const QString &ip)
{
    if (m_txServerIp != ip) {
        m_txServerIp = ip;
        emit txServerIpChanged();
    }
}

void ConfigManager::setRxHostIp(const QString &ip)
{
    if (m_rxHostIp != ip) {
        m_rxHostIp = ip;
        emit rxHostIpChanged();
    }
}

void ConfigManager::setResolutionIndex(int index)
{
    if (index < 0 || index >= m_resolutionOptions.size()) {
        index = 0;
    }
    if (m_resolutionIndex != index) {
        m_resolutionIndex = index;
        applyResolutionSelection();
        emit resolutionIndexChanged();
    }
}

void ConfigManager::setFramerateIndex(int index)
{
    if (index < 0 || index >= m_framerateValues.size()) {
        index = 0;
    }
    if (m_framerateIndex != index) {
        m_framerateIndex = index;
        applyFramerateSelection();
        emit framerateIndexChanged();
    }
}

void ConfigManager::setRotate(int rotate)
{
    rotate = qBound(0, rotate, 3);
    if (m_rotate != rotate) {
        m_rotate = rotate;
        // When rotate changes, we may need to swap width/height
        applyResolutionSelection();
        emit rotateChanged();
    }
}

void ConfigManager::setSerialPort(const QString &port)
{
    if (m_serialPort != port) {
        m_serialPort = port;
        emit serialPortChanged();
    }
}

void ConfigManager::setGrpcServerAddress(const QString &address)
{
    if (m_grpcServerAddress != address) {
        m_grpcServerAddress = address;
        emit grpcServerAddressChanged();
    }
}

void ConfigManager::setUseGrpc(bool use)
{
    if (m_useGrpc != use) {
        m_useGrpc = use;
        emit useGrpcChanged();
    }
}

void ConfigManager::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "ConfigManager status:" << msg;
    }
}

void ConfigManager::setIsBusy(bool busy)
{
    if (m_isBusy != busy) {
        m_isBusy = busy;
        emit isBusyChanged();
    }
}

bool ConfigManager::rotateIsHorizontal(int rotate)
{
    return (rotate % 2) != 0; // rotate = 1 or 3
}

void ConfigManager::applyResolutionSelection()
{
    if (m_resolutionIndex < 0 || m_resolutionIndex >= (int)(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]))) {
        m_resolutionIndex = 0;
    }

    m_width = kResolutionPresets[m_resolutionIndex][0];
    m_height = kResolutionPresets[m_resolutionIndex][1];

    // Swap width/height if rotate is horizontal
    if (rotateIsHorizontal(m_rotate)) {
        int tmp = m_width;
        m_width = m_height;
        m_height = tmp;
    }

    emit resolutionChanged();
}

void ConfigManager::applyFramerateSelection()
{
    if (m_framerateIndex < 0 || m_framerateIndex >= m_framerateValues.size()) {
        m_framerateIndex = 0;
    }
    m_framerate = m_framerateValues[m_framerateIndex];
    emit framerateChanged();
}

void ConfigManager::loadSettings()
{
    m_txServerIp = m_settings->value("txServerIp", "192.168.4.1").toString();
    m_rxHostIp = m_settings->value("rxHostIp", "127.0.0.1").toString();
    m_resolutionIndex = m_settings->value("resolutionIndex", 0).toInt();
    m_framerateIndex = m_settings->value("framerateIndex", 0).toInt();
    m_rotate = m_settings->value("rotate", 0).toInt();
    m_grpcServerAddress = m_settings->value("grpcServerAddress", "192.168.4.1:50051").toString();
    m_useGrpc = m_settings->value("useGrpc", true).toBool();

    applyResolutionSelection();
    applyFramerateSelection();

    emit txServerIpChanged();
    emit rxHostIpChanged();
    emit resolutionIndexChanged();
    emit framerateIndexChanged();
    emit rotateChanged();
    emit grpcServerAddressChanged();
    emit useGrpcChanged();

    qDebug() << "Settings loaded - TX:" << m_txServerIp << "RX:" << m_rxHostIp
             << "Resolution:" << m_width << "x" << m_height
             << "Framerate:" << m_framerate << "Rotate:" << m_rotate
             << "gRPC:" << m_grpcServerAddress << "useGrpc:" << m_useGrpc;
}

void ConfigManager::saveSettings()
{
    m_settings->setValue("txServerIp", m_txServerIp);
    m_settings->setValue("rxHostIp", m_rxHostIp);
    m_settings->setValue("resolutionIndex", m_resolutionIndex);
    m_settings->setValue("framerateIndex", m_framerateIndex);
    m_settings->setValue("rotate", m_rotate);
    m_settings->setValue("grpcServerAddress", m_grpcServerAddress);
    m_settings->setValue("useGrpc", m_useGrpc);
    m_settings->sync();

    qDebug() << "Settings saved to local storage";
}

void ConfigManager::testConnection()
{
    if (m_isBusy) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Testing connection...");

    // Trigger worker thread
    emit startTestConnection(m_txServerIp, m_txHttpPort);
}

void ConfigManager::onTestConnectionFinished(bool success)
{
    if (success) {
        setStatusMessage("Connection OK");
    } else {
        setStatusMessage("Connection Failed");
    }

    // Update camera connected state
    if (m_cameraConnected != success) {
        m_cameraConnected = success;
        emit cameraConnectedChanged();
    }

    setIsBusy(false);
    emit connectionTestResult(success);
}

void ConfigManager::saveConfig()
{
    if (m_isBusy) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Saving configuration...");

    // Trigger worker thread
    emit startSaveConfig(m_serialPort, m_txServerIp, m_txHttpPort,
                         m_rxHostIp, m_rxStreamPort,
                         m_width, m_height, m_framerate, m_rotate);
}

void ConfigManager::onSaveConfigFinished(bool success, const QString &statusMessage)
{
    Q_UNUSED(success);

    // Save to local settings (on main thread, as QSettings is thread-safe for reads but we're writing)
    saveSettings();

    // Mark direction as saved
    if (!m_directionSaved) {
        m_directionSaved = true;
        emit directionSavedChanged();
    }

    setStatusMessage(statusMessage);
    setIsBusy(false);
    emit configSaved();
}

void ConfigManager::loadConfigFromCamera()
{
    if (m_serialPort.isEmpty()) {
        setStatusMessage("No camera connected");
        return;
    }

    if (m_isBusy) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Loading config from camera...");

    // Trigger worker thread
    emit startLoadConfig(m_serialPort);
}

void ConfigManager::onLoadConfigFinished(bool success, const QString &rxHostIp, int resolutionIndex, int framerateIndex)
{
    if (success) {
        if (!rxHostIp.isEmpty()) {
            setRxHostIp(rxHostIp);
        }
        if (resolutionIndex >= 0) {
            setResolutionIndex(resolutionIndex);
        }
        if (framerateIndex >= 0) {
            setFramerateIndex(framerateIndex);
        }
        setStatusMessage("Config loaded from camera");
    } else {
        setStatusMessage("Failed to load config from camera");
    }

    setIsBusy(false);
}

QString ConfigManager::detectLocalIp()
{
    // Get list of all network interfaces and find the best local IP
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (const QNetworkInterface &iface : interfaces) {
        // Skip loopback and down interfaces
        if (iface.flags() & QNetworkInterface::IsLoopBack) {
            continue;
        }
        if (!(iface.flags() & QNetworkInterface::IsUp)) {
            continue;
        }
        if (!(iface.flags() & QNetworkInterface::IsRunning)) {
            continue;
        }

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            QHostAddress addr = entry.ip();

            // Only IPv4
            if (addr.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }

            // Skip localhost
            if (addr.isLoopback()) {
                continue;
            }

            QString ipStr = addr.toString();

            // Prefer 192.168.x.x addresses (common for home networks)
            if (ipStr.startsWith("192.168.")) {
                LogManager::log(QString("Detected local IP: %1 (interface: %2)").arg(ipStr, iface.humanReadableName()));
                return ipStr;
            }
        }
    }

    // Second pass: accept any non-loopback IPv4
    for (const QNetworkInterface &iface : interfaces) {
        if (iface.flags() & QNetworkInterface::IsLoopBack) {
            continue;
        }
        if (!(iface.flags() & QNetworkInterface::IsUp)) {
            continue;
        }

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            QHostAddress addr = entry.ip();

            if (addr.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }

            if (addr.isLoopback()) {
                continue;
            }

            QString ipStr = addr.toString();
            LogManager::log(QString("Detected local IP: %1 (interface: %2)").arg(ipStr, iface.humanReadableName()));
            return ipStr;
        }
    }

    LogManager::log("Could not detect local IP, using 127.0.0.1");
    return "127.0.0.1";
}

void ConfigManager::setDirectionSaved(bool saved)
{
    if (m_directionSaved != saved) {
        m_directionSaved = saved;
        emit directionSavedChanged();
    }
}
