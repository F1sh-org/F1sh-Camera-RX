#ifndef MDNSMANAGER_H
#define MDNSMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Structure to hold discovered camera info
struct CameraInfo {
    QString name;
    QString instanceFqdn;
    QString hostname;
    QString ip;
    int port = 0;           // Stream port from service
    int controlPort = 50051; // gRPC control port
    QString protocol;       // udp, tcp
    QString encoding;       // h264, h265
};

class MdnsManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString cameraIp READ cameraIp NOTIFY cameraIpChanged)
    Q_PROPERTY(QString cameraHostname READ cameraHostname NOTIFY cameraHostnameChanged)
    Q_PROPERTY(int cameraPort READ cameraPort NOTIFY cameraPortChanged)
    Q_PROPERTY(int controlPort READ controlPort NOTIFY controlPortChanged)
    Q_PROPERTY(QString protocol READ protocol NOTIFY protocolChanged)
    Q_PROPERTY(QString encoding READ encoding NOTIFY encodingChanged)
    Q_PROPERTY(bool isDiscovering READ isDiscovering NOTIFY isDiscoveringChanged)
    Q_PROPERTY(bool cameraFound READ cameraFound NOTIFY cameraFoundChanged)
    Q_PROPERTY(QVariantList discoveredCameras READ discoveredCameras NOTIFY discoveredCamerasChanged)
    Q_PROPERTY(int cameraCount READ cameraCount NOTIFY cameraCountChanged)

public:
    explicit MdnsManager(QObject *parent = nullptr);
    ~MdnsManager();

    QString cameraIp() const { return m_cameraIp; }
    QString cameraHostname() const { return m_cameraHostname; }
    int cameraPort() const { return m_cameraPort; }
    int controlPort() const { return m_controlPort; }
    QString protocol() const { return m_protocol; }
    QString encoding() const { return m_encoding; }
    bool isDiscovering() const { return m_isDiscovering; }
    bool cameraFound() const { return m_cameraFound; }
    QVariantList discoveredCameras() const { return m_discoveredCameras; }
    int cameraCount() const { return m_cameras.size(); }

    Q_INVOKABLE void startDiscovery();
    Q_INVOKABLE void stopDiscovery();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectCamera(int index);

signals:
    void cameraIpChanged();
    void cameraHostnameChanged();
    void cameraPortChanged();
    void controlPortChanged();
    void protocolChanged();
    void encodingChanged();
    void isDiscoveringChanged();
    void cameraFoundChanged();
    void discoveredCamerasChanged();
    void cameraCountChanged();
    void discoveryFinished(bool found, const QString &ip, int port);
    void multipleCamerasFound();  // Emitted when more than one camera found

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onDiscoveryTimeout();

private:
    void parseDiscoveryOutput(const QString &output);
    void parseTxtRecord(const QString &txt, CameraInfo &info);
    void finalizeDiscoveryResults();
#ifdef Q_OS_WIN
    bool discoverWindowsNative();
#endif
    void setCameraIp(const QString &ip);
    void setCameraHostname(const QString &hostname);
    void setCameraPort(int port);
    void setControlPort(int port);
    void setProtocol(const QString &protocol);
    void setEncoding(const QString &encoding);
    void setIsDiscovering(bool discovering);
    void setCameraFound(bool found);
    void updateDiscoveredCamerasList();
    void applyCamera(const CameraInfo &camera);

    QProcess *m_process = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QString m_cameraIp;
    QString m_cameraHostname;
    int m_cameraPort = 0;
    int m_controlPort = 50051;
    QString m_protocol = "udp";
    QString m_encoding = "h264";
    bool m_isDiscovering = false;
    bool m_cameraFound = false;

    QList<CameraInfo> m_cameras;
    QVariantList m_discoveredCameras;

    static const QString kServiceType;
};

#endif // MDNSMANAGER_H
