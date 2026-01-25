#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QMutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class SerialPortWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialPortWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void doDetection();

signals:
    void detectionFinished(bool cameraConnected, const QString &connectedPort, const QStringList &cameras);

private:
    bool probePort(const QString &portName);
    QStringList listAvailablePorts();
};

class SerialPortManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool cameraConnected READ cameraConnected NOTIFY cameraConnectedChanged)
    Q_PROPERTY(QString connectedPort READ connectedPort NOTIFY connectedPortChanged)
    Q_PROPERTY(QStringList availableCameras READ availableCameras NOTIFY availableCamerasChanged)

public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager();

    bool cameraConnected() const { return m_cameraConnected; }
    QString connectedPort() const { return m_connectedPort; }
    QStringList availableCameras() const { return m_availableCameras; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startAutoDetect();
    Q_INVOKABLE void stopAutoDetect();

signals:
    void cameraConnectedChanged();
    void connectedPortChanged();
    void availableCamerasChanged();
    void startDetection();

private slots:
    void onDetectionFinished(bool cameraConnected, const QString &connectedPort, const QStringList &cameras);
    void triggerDetection();

private:
    bool m_cameraConnected = false;
    QString m_connectedPort;
    QStringList m_availableCameras;
    QTimer *m_autoDetectTimer = nullptr;
    QThread *m_workerThread = nullptr;
    SerialPortWorker *m_worker = nullptr;
    bool m_detectionInProgress = false;
};

#endif // SERIALPORTMANAGER_H
