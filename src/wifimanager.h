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

// Worker class that runs on a background thread
class WifiWorker : public QObject
{
    Q_OBJECT
public:
    explicit WifiWorker(QObject *parent = nullptr) : QObject(parent) {}
    
public slots:
    void doScan(const QString &serialPort);
    
signals:
    void scanFinished(const QVariantList &networks, bool success, const QString &errorMessage);
    
private:
    QByteArray sendSerialCommand(const QString &portName, const QByteArray &command);
};

class WifiManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(QString serialPort READ serialPort WRITE setSerialPort NOTIFY serialPortChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit WifiManager(QObject *parent = nullptr);
    ~WifiManager();

    QVariantList networks() const { return m_networks; }
    bool isScanning() const { return m_scanInProgress; }
    QString serialPort() const { return m_serialPort; }
    QString errorMessage() const { return m_errorMessage; }
    
    void setSerialPort(const QString &port);

    Q_INVOKABLE void refresh();

signals:
    void networksChanged();
    void isScanningChanged();
    void serialPortChanged();
    void errorMessageChanged();
    void startScan(const QString &serialPort);

private slots:
    void onScanFinished(const QVariantList &networks, bool success, const QString &errorMessage);

private:
    QVariantList m_networks;
    QString m_serialPort;
    QString m_errorMessage;
    bool m_scanInProgress = false;
    
    QThread *m_workerThread = nullptr;
    WifiWorker *m_worker = nullptr;
};

#endif // WIFIMANAGER_H
