#include "serialportmanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QSerialPort>
#include <QSerialPortInfo>

static const QByteArray kProbeMessage = "{\"status\":1}\n";
static const QByteArray kExpectedResponse = "{\"status\":1}";

// ============ SerialPortWorker Implementation ============

QStringList SerialPortWorker::listAvailablePorts()
{
    QStringList ports;

    // Use Qt's cross-platform serial port enumeration
    const QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : availablePorts) {
        // Filter to USB serial ports (typically our F1sh Camera)
        // On Windows: COM ports
        // On macOS: /dev/tty.usbserial* or /dev/tty.usbmodem*
        // On Linux: /dev/ttyUSB* or /dev/ttyACM*
        QString portName = info.portName();

#ifdef _WIN32
        // On Windows, use the port name directly (e.g., "COM3")
        ports.append(portName);
#else
        // On macOS/Linux, use the system location (full path)
        ports.append(info.systemLocation());
#endif
    }

    return ports;
}

bool SerialPortWorker::probePort(const QString &portName)
{
    LogManager::log(QString("Probing %1").arg(portName));

    QSerialPort serial;
    serial.setPortName(portName);
    serial.setBaudRate(QSerialPort::Baud115200);
    serial.setDataBits(QSerialPort::Data8);
    serial.setParity(QSerialPort::NoParity);
    serial.setStopBits(QSerialPort::OneStop);
    serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial.open(QIODevice::ReadWrite)) {
        LogManager::log(QString("  Failed to open %1: %2").arg(portName, serial.errorString()));
        return false;
    }

    LogManager::log(QString("  Successfully opened %1").arg(portName));

    // Send probe message
    LogManager::log(QString("  Sending probe: %1").arg(QString::fromUtf8(kProbeMessage).trimmed()));
    qint64 written = serial.write(kProbeMessage);
    serial.flush();
    LogManager::log(QString("  Sent %1 bytes").arg(written));

    // Wait for response with timeout
    if (!serial.waitForReadyRead(400)) {
        LogManager::log(QString("  No response from %1 (timeout)").arg(portName));
        serial.close();
        return false;
    }

    // Read response
    QByteArray response = serial.readAll();

    // Continue reading if more data is available
    while (serial.waitForReadyRead(50)) {
        response.append(serial.readAll());
    }

    serial.close();

    if (response.isEmpty()) {
        LogManager::log(QString("  No response from %1").arg(portName));
        return false;
    }

    LogManager::log(QString("  Received %1 bytes: %2").arg(response.size()).arg(QString::fromUtf8(response).trimmed()));

    bool found = response.contains(kExpectedResponse);

    if (found) {
        LogManager::log(QString("  CAMERA DETECTED on %1!").arg(portName));
    } else {
        LogManager::log(QString("  Response doesn't match expected: %1").arg(QString::fromUtf8(kExpectedResponse)));
    }

    return found;
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
