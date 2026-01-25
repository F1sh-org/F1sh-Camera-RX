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

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

// Resolution presets (width, height)
static const int kResolutionPresets[][2] = {
    { 1280, 720 },
    { 720, 1280 }
};

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings("F1sh", "CameraRX", this))
{
    // Load saved settings on startup
    loadSettings();
}

ConfigManager::~ConfigManager()
{
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
    
    applyResolutionSelection();
    applyFramerateSelection();
    
    emit txServerIpChanged();
    emit rxHostIpChanged();
    emit resolutionIndexChanged();
    emit framerateIndexChanged();
    emit rotateChanged();
    
    qDebug() << "Settings loaded - TX:" << m_txServerIp << "RX:" << m_rxHostIp 
             << "Resolution:" << m_width << "x" << m_height 
             << "Framerate:" << m_framerate << "Rotate:" << m_rotate;
}

void ConfigManager::saveSettings()
{
    m_settings->setValue("txServerIp", m_txServerIp);
    m_settings->setValue("rxHostIp", m_rxHostIp);
    m_settings->setValue("resolutionIndex", m_resolutionIndex);
    m_settings->setValue("framerateIndex", m_framerateIndex);
    m_settings->setValue("rotate", m_rotate);
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
    LogManager::log(QString("Testing connection to TX server at %1...").arg(m_txServerIp));
    
    bool success = httpTestConnection();
    
    if (success) {
        setStatusMessage("Connection OK");
        LogManager::log("TX server connection successful");
    } else {
        setStatusMessage("Connection Failed");
        LogManager::log("TX server connection failed");
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
    setStatusMessage("Testing connection...");
    LogManager::log("Saving configuration...");
    
    // Test connection first
    if (!httpTestConnection()) {
        setStatusMessage("Cannot connect to TX server");
        LogManager::log("Save config failed: Cannot connect to TX server");
        setIsBusy(false);
        return;
    }
    
    setStatusMessage("Sending configuration...");
    LogManager::log(QString("Sending config: %1x%2 @ %3fps, rotate=%4").arg(m_width).arg(m_height).arg(m_framerate).arg(m_rotate));
    
    // Send configuration to TX server
    if (!httpSendConfig()) {
        setStatusMessage("Failed to configure TX server");
        LogManager::log("Failed to configure TX server");
        setIsBusy(false);
        return;
    }
    
    // Send rotate to local RX server
    if (!httpSendRxRotate(m_rotate)) {
        setStatusMessage("Failed to set rotate on RX server");
        LogManager::log("Warning: Failed to set rotate on RX server");
        // Don't return - continue anyway
    }
    
    // Save to local settings
    saveSettings();
    
    // Mark direction as saved
    if (!m_directionSaved) {
        m_directionSaved = true;
        emit directionSavedChanged();
    }
    
    setStatusMessage("Configuration saved to TX server");
    LogManager::log("Configuration saved successfully");
    setIsBusy(false);
    emit configSaved();
}

void ConfigManager::loadConfigFromCamera()
{
    if (m_serialPort.isEmpty()) {
        setStatusMessage("No camera connected");
        return;
    }
    
    setIsBusy(true);
    setStatusMessage("Loading config from camera...");
    
    fetchConfigFromSerial();
    
    setIsBusy(false);
}

// ============ HTTP Operations ============

#ifdef _WIN32
// Simple HTTP GET/POST using WinHTTP
static bool doHttpRequest(const QString &host, int port, const QString &path, 
                          const QString &method, const QByteArray &body,
                          QByteArray &response)
{
    response.clear();
    
    HINTERNET hSession = WinHttpOpen(L"F1shCameraRX/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        qWarning() << "WinHttpOpen failed";
        return false;
    }
    
    std::wstring wHost = host.toStdWString();
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        qWarning() << "WinHttpConnect failed";
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    std::wstring wPath = path.toStdWString();
    std::wstring wMethod = method.toStdWString();
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        qWarning() << "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Set timeouts (5 seconds)
    DWORD timeout = 5000;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    
    BOOL bResults = FALSE;
    if (body.isEmpty()) {
        bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    } else {
        const wchar_t *headers = L"Content-Type: application/json";
        bResults = WinHttpSendRequest(hRequest, headers, -1,
                                      (LPVOID)body.constData(), body.size(), body.size(), 0);
    }
    
    if (!bResults) {
        qWarning() << "WinHttpSendRequest failed:" << GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        qWarning() << "WinHttpReceiveResponse failed:" << GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Check status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        NULL, &statusCode, &statusCodeSize, NULL);
    
    if (statusCode != 200) {
        qWarning() << "HTTP status code:" << statusCode;
    }
    
    // Read response
    DWORD dwSize = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            break;
        }
        
        if (dwSize > 0) {
            char *buffer = new char[dwSize + 1];
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded)) {
                response.append(buffer, dwDownloaded);
            }
            delete[] buffer;
        }
    } while (dwSize > 0);
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return statusCode == 200;
}
#endif

bool ConfigManager::httpTestConnection()
{
#ifdef _WIN32
    QByteArray response;
    bool success = doHttpRequest(m_txServerIp, m_txHttpPort, "/health", "GET", QByteArray(), response);
    
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
#else
    return false;
#endif
}

bool ConfigManager::httpSendConfig()
{
#ifdef _WIN32
    // Create JSON config
    QJsonObject config;
    config["host"] = m_rxHostIp;
    config["port"] = m_rxStreamPort;
    config["width"] = m_width;
    config["height"] = m_height;
    config["framerate"] = m_framerate;
    
    QJsonDocument doc(config);
    QByteArray body = doc.toJson(QJsonDocument::Compact);
    
    qDebug() << "Sending config:" << body;
    
    QByteArray response;
    bool success = doHttpRequest(m_txServerIp, m_txHttpPort, "/config", "POST", body, response);
    
    if (success) {
        qDebug() << "Config response:" << response;
    }
    
    return success;
#else
    return false;
#endif
}

bool ConfigManager::httpSendRxRotate(int rotate)
{
#ifdef _WIN32
    QJsonObject obj;
    obj["rotate"] = rotate;
    
    QJsonDocument doc(obj);
    QByteArray body = doc.toJson(QJsonDocument::Compact);
    
    QByteArray response;
    // RX server runs on port 8889
    bool success = doHttpRequest(m_rxHostIp, 8889, "/rotate", "POST", body, response);
    
    return success;
#else
    return false;
#endif
}

// ============ Serial Operations ============

QByteArray ConfigManager::sendSerialCommand(const QString &command)
{
#ifdef _WIN32
    if (m_serialPort.isEmpty()) {
        return QByteArray();
    }
    
    QString devicePath;
    if (m_serialPort.startsWith("COM")) {
        int num = m_serialPort.mid(3).toInt();
        if (num >= 10) {
            devicePath = QString("\\\\.\\%1").arg(m_serialPort);
        } else {
            devicePath = m_serialPort;
        }
    } else {
        devicePath = m_serialPort;
    }
    
    HANDLE h = CreateFileA(devicePath.toLatin1().constData(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        qWarning() << "Failed to open serial port:" << m_serialPort;
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
    timeouts.ReadTotalTimeoutConstant = 4000;
    timeouts.WriteTotalTimeoutConstant = 200;
    SetCommTimeouts(h, &timeouts);
    
    // Send command
    QByteArray cmdBytes = command.toUtf8();
    DWORD written = 0;
    WriteFile(h, cmdBytes.constData(), cmdBytes.size(), &written, NULL);
    
    // Read response
    QByteArray response;
    char readbuf[4096] = {0};
    DWORD bytesRead = 0;
    
    while (ReadFile(h, readbuf, sizeof(readbuf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        response.append(readbuf, bytesRead);
        if (response.contains('}')) {
            break;
        }
    }
    
    CloseHandle(h);
    return response;
#else
    Q_UNUSED(command);
    return QByteArray();
#endif
}

void ConfigManager::fetchConfigFromSerial()
{
    // Send status 5 command to get TX config
    QByteArray response = sendSerialCommand("{\"status\":5}\n");
    
    if (response.isEmpty()) {
        setStatusMessage("No response from camera");
        return;
    }
    
    qDebug() << "TX config response:" << response;
    
    // Parse JSON response
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response, &error);
    
    if (error.error != QJsonParseError::NoError) {
        setStatusMessage("Invalid response from camera");
        return;
    }
    
    QJsonObject root = doc.object();
    
    // Check for status 5 response
    if (root["status"].toInt() != 5) {
        setStatusMessage("Unexpected response from camera");
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
        setStatusMessage("No config data from camera");
        return;
    }
    
    // Update config from camera response
    if (configObj.contains("host")) {
        setRxHostIp(configObj["host"].toString());
    }
    if (configObj.contains("width") && configObj.contains("height")) {
        int w = configObj["width"].toInt();
        int h = configObj["height"].toInt();
        
        // Find matching resolution preset
        for (int i = 0; i < (int)(sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0])); i++) {
            if ((kResolutionPresets[i][0] == w && kResolutionPresets[i][1] == h) ||
                (kResolutionPresets[i][0] == h && kResolutionPresets[i][1] == w)) {
                setResolutionIndex(i);
                break;
            }
        }
    }
    if (configObj.contains("framerate")) {
        int fr = configObj["framerate"].toInt();
        for (int i = 0; i < m_framerateValues.size(); i++) {
            if (m_framerateValues[i] == fr) {
                setFramerateIndex(i);
                break;
            }
        }
    }
    
    setStatusMessage("Config loaded from camera");
}
