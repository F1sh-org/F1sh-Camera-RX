#include "serialportmanager.h"
#include "logmanager.h"
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#endif

static const QByteArray kProbeMessage = "{\"status\":1}\n";
static const QByteArray kExpectedResponse = "{\"status\":1}";

// ============ SerialPortWorker Implementation ============

QStringList SerialPortWorker::listAvailablePorts()
{
    QStringList ports;
    
#ifdef _WIN32
    // Enumerate COM ports on Windows using QueryDosDevice
    for (int i = 1; i <= 20; ++i) {
        QString portName = QString("COM%1").arg(i);
        QByteArray dosName = portName.toLatin1();
        char target[256];
        
        if (QueryDosDeviceA(dosName.constData(), target, sizeof(target)) != 0) {
            ports.append(portName);
            continue;
        }
        
        // Also try opening the port to check if it exists
        QString devicePath = QString("\\\\.\\%1").arg(portName);
        HANDLE h = CreateFileA(devicePath.toLatin1().constData(),
                               GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            ports.append(portName);
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION) {
                // Port exists but is in use
                ports.append(portName);
            }
        }
    }
#endif
    
    return ports;
}

bool SerialPortWorker::probePort(const QString &portName)
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

    LogManager::log(QString("Probing %1 (path: %2)").arg(portName, devicePath));

    HANDLE h = CreateFileA(devicePath.toLatin1().constData(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LogManager::log(QString("  Failed to open %1: error code %2").arg(portName).arg(err));
        return false;
    }

    LogManager::log(QString("  Successfully opened %1").arg(portName));

    // Configure serial port
    DCB dcb;
    memset(&dcb, 0, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        LogManager::log(QString("  Failed to get comm state for %1").arg(portName));
        CloseHandle(h);
        return false;
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
        LogManager::log(QString("  Failed to set comm state for %1").arg(portName));
        CloseHandle(h);
        return false;
    }

    // Set timeouts
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 400;
    timeouts.WriteTotalTimeoutConstant = 200;
    SetCommTimeouts(h, &timeouts);

    // Send probe message
    LogManager::log(QString("  Sending probe: %1").arg(QString::fromUtf8(kProbeMessage).trimmed()));
    DWORD written = 0;
    WriteFile(h, kProbeMessage.constData(), kProbeMessage.size(), &written, NULL);
    LogManager::log(QString("  Sent %1 bytes").arg(written));

    // Read response
    char readbuf[1024] = {0};
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(h, readbuf, sizeof(readbuf) - 1, &bytesRead, NULL);

    CloseHandle(h);

    if (!ok || bytesRead == 0) {
        LogManager::log(QString("  No response from %1 (bytesRead=%2, ok=%3)").arg(portName).arg(bytesRead).arg(ok));
        return false;
    }

    readbuf[bytesRead] = '\0';
    LogManager::log(QString("  Received %1 bytes: %2").arg(bytesRead).arg(QString::fromUtf8(readbuf).trimmed()));

    bool found = strstr(readbuf, kExpectedResponse.constData()) != nullptr;

    if (found) {
        LogManager::log(QString("  CAMERA DETECTED on %1!").arg(portName));
    } else {
        LogManager::log(QString("  Response doesn't match expected: %1").arg(QString::fromUtf8(kExpectedResponse)));
    }

    return found;
#else
    Q_UNUSED(portName);
    return false;
#endif
}

void SerialPortWorker::doDetection()
{
    QStringList cameras;
    QString foundPort;

    // Get list of available serial ports
    QStringList ports = listAvailablePorts();
    LogManager::log(QString("Scanning %1 serial ports: %2").arg(ports.size()).arg(ports.join(", ")));

    for (const QString &portName : ports) {
        if (probePort(portName)) {
            cameras.append(portName);
            LogManager::log(QString("Camera found on %1").arg(portName));
            if (foundPort.isEmpty()) {
                foundPort = portName;
            }
        }
    }

    if (cameras.isEmpty()) {
        LogManager::log("No camera found on any serial port");
    }

    emit detectionFinished(!cameras.isEmpty(), foundPort, cameras);
}

// ============ SerialPortManager Implementation ============

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent)
    , m_autoDetectTimer(new QTimer(this))
    , m_workerThread(new QThread(this))
    , m_worker(new SerialPortWorker())
{
    // Move worker to background thread
    m_worker->moveToThread(m_workerThread);
    
    // Connect signals
    connect(m_autoDetectTimer, &QTimer::timeout, this, &SerialPortManager::triggerDetection);
    connect(this, &SerialPortManager::startDetection, m_worker, &SerialPortWorker::doDetection);
    connect(m_worker, &SerialPortWorker::detectionFinished, this, &SerialPortManager::onDetectionFinished);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    
    // Start worker thread
    m_workerThread->start();
    
    // Initial detection
    triggerDetection();
}

SerialPortManager::~SerialPortManager()
{
    stopAutoDetect();
    m_workerThread->quit();
    m_workerThread->wait();
}

void SerialPortManager::refresh()
{
    triggerDetection();
}

void SerialPortManager::startAutoDetect()
{
    if (!m_autoDetectTimer->isActive()) {
        m_autoDetectTimer->start(2000); // Check every 2 seconds
    }
}

void SerialPortManager::stopAutoDetect()
{
    m_autoDetectTimer->stop();
}

void SerialPortManager::pauseAutoDetect()
{
    m_autoDetectPaused = true;
    LogManager::log("Auto-detection paused");
}

void SerialPortManager::resumeAutoDetect()
{
    m_autoDetectPaused = false;
    LogManager::log("Auto-detection resumed");
}

void SerialPortManager::triggerDetection()
{
    // Don't start new detection if one is already in progress or if paused
    if (m_detectionInProgress || m_autoDetectPaused) {
        return;
    }
    m_detectionInProgress = true;
    emit startDetection();
}

void SerialPortManager::onDetectionFinished(bool cameraConnected, const QString &connectedPort, const QStringList &cameras)
{
    m_detectionInProgress = false;
    
    // Update state
    bool wasConnected = m_cameraConnected;
    QString oldPort = m_connectedPort;
    QStringList oldCameras = m_availableCameras;
    
    m_cameraConnected = cameraConnected;
    m_connectedPort = connectedPort;
    m_availableCameras = cameras;
    
    // Emit signals if changed
    if (m_cameraConnected != wasConnected) {
        emit cameraConnectedChanged();
    }
    if (m_connectedPort != oldPort) {
        emit connectedPortChanged();
    }
    if (m_availableCameras != oldCameras) {
        emit availableCamerasChanged();
    }
}
