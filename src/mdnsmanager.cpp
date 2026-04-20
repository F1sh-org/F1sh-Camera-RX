#include "mdnsmanager.h"
#include "logmanager.h"
#include <QDebug>
#include <QRegularExpression>
#include <QHostInfo>
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windns.h>
#endif

const QString MdnsManager::kServiceType = "_f1sh-camera._tcp";

MdnsManager::MdnsManager(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_timeoutTimer(new QTimer(this))
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MdnsManager::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &MdnsManager::onProcessError);
    connect(m_timeoutTimer, &QTimer::timeout,
            this, &MdnsManager::onDiscoveryTimeout);

    m_timeoutTimer->setSingleShot(true);

    // Start discovery automatically on construction
    QTimer::singleShot(500, this, &MdnsManager::startDiscovery);
}

MdnsManager::~MdnsManager()
{
    stopDiscovery();
}

void MdnsManager::startDiscovery()
{
    if (m_isDiscovering) {
        return;
    }

    // Clear previous results
    m_cameras.clear();
    updateDiscoveredCamerasList();

    setIsDiscovering(true);
    setCameraFound(false);
    LogManager::log("Starting mDNS discovery for _f1sh-camera._tcp...");

    // Use platform-specific mDNS browse command
#ifdef Q_OS_WIN
    if (discoverWindowsNative()) {
        finalizeDiscoveryResults();
        return;
    }
    setIsDiscovering(false);
    emit discoveryFinished(false, QString(), 0);
    return;
#elif defined(Q_OS_MACOS)
    // macOS: use dns-sd -Z to get full zone info including TXT records
    m_process->start("dns-sd", QStringList() << "-Z" << "_f1sh-camera._tcp" << "local");
#else
    // Linux: use avahi-browse with TXT records
    m_process->start("avahi-browse", QStringList() << "-rpt" << "_f1sh-camera._tcp");
#endif

    // Set timeout for discovery (5 seconds)
    m_timeoutTimer->start(5000);
}

void MdnsManager::stopDiscovery()
{
    m_timeoutTimer->stop();

    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(1000)) {
            m_process->kill();
        }
    }

    setIsDiscovering(false);
}

void MdnsManager::refresh()
{
    stopDiscovery();
    startDiscovery();
}

void MdnsManager::selectCamera(int index)
{
    if (index < 0 || index >= m_cameras.size()) {
        LogManager::log(QString("Invalid camera index: %1").arg(index));
        return;
    }

    const CameraInfo &camera = m_cameras.at(index);
    applyCamera(camera);
    LogManager::log(QString("Selected camera: %1 (%2)").arg(camera.name, camera.ip));
}

void MdnsManager::applyCamera(const CameraInfo &camera)
{
    setCameraHostname(camera.hostname);
    setCameraIp(camera.ip);
    setCameraPort(camera.port);
    setControlPort(camera.controlPort);
    setProtocol(camera.protocol);
    setEncoding(camera.encoding);
    setCameraFound(true);
}

void MdnsManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    QString errorOutput = QString::fromUtf8(m_process->readAllStandardError());

    if (!output.isEmpty()) {
        LogManager::log(QString("mDNS output:\n%1").arg(output.left(1000)));
    }
    if (!errorOutput.isEmpty()) {
        LogManager::log(QString("mDNS error: %1").arg(errorOutput.left(200)));
    }

    parseDiscoveryOutput(output);

    m_timeoutTimer->stop();
    setIsDiscovering(false);
    finalizeDiscoveryResults();
}

void MdnsManager::onProcessError(QProcess::ProcessError error)
{
    QString errorStr;
    switch (error) {
        case QProcess::FailedToStart:
            errorStr = "mDNS tool not found (dns-sd or avahi-browse)";
            break;
        case QProcess::Crashed:
            errorStr = "mDNS tool crashed";
            break;
        case QProcess::Timedout:
            errorStr = "mDNS discovery timed out";
            break;
        default:
            errorStr = "mDNS discovery failed";
            break;
    }

    LogManager::log(errorStr);
    m_timeoutTimer->stop();
    setIsDiscovering(false);
    emit discoveryFinished(false, QString(), 0);
}

void MdnsManager::onDiscoveryTimeout()
{
    LogManager::log("mDNS discovery timeout - reading current results...");

    // Read any output we have
    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    if (!output.isEmpty()) {
        LogManager::log(QString("mDNS output on timeout:\n%1").arg(output.left(1000)));
        parseDiscoveryOutput(output);
    }

    stopDiscovery();
    finalizeDiscoveryResults();
}

void MdnsManager::parseTxtRecord(const QString &txt, CameraInfo &info)
{
    // Parse TXT record entries like: protocol=udp encoding=h264 control_port=50051
    QRegularExpression entryRegex(R"((\w+)=(\S+))");
    QRegularExpressionMatchIterator it = entryRegex.globalMatch(txt);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString key = match.captured(1).toLower();
        QString value = match.captured(2);

        if (key == "protocol") {
            info.protocol = value;
        } else if (key == "encoding") {
            info.encoding = value;
        } else if (key == "control_port") {
            info.controlPort = value.toInt();
        }
    }
}

void MdnsManager::finalizeDiscoveryResults()
{
    if (m_cameras.size() == 1) {
        applyCamera(m_cameras.first());
        emit discoveryFinished(true, m_cameraIp, m_cameraPort);
        return;
    }

    if (m_cameras.size() > 1) {
        LogManager::log(QString("Found %1 cameras - user selection required").arg(m_cameras.size()));
        emit multipleCamerasFound();
        emit discoveryFinished(true, QString(), 0);
        return;
    }

    LogManager::log("No camera found via mDNS");
    emit discoveryFinished(false, QString(), 0);
}

#ifdef Q_OS_WIN
bool MdnsManager::discoverWindowsNative()
{
    DNS_RECORD* browseResults = nullptr;
    DNS_STATUS browseStatus = DnsQuery_A(
        "_f1sh-camera._tcp.local",
        DNS_TYPE_PTR,
        DNS_QUERY_MULTICAST_ONLY,
        nullptr,
        &browseResults,
        nullptr);

    if (browseStatus != ERROR_SUCCESS) {
        LogManager::log(QString("Windows mDNS browse failed: %1").arg(static_cast<int>(browseStatus)));
        return false;
    }

    QMap<QString, CameraInfo> cameraMap;

    for (DNS_RECORD* rec = browseResults; rec; rec = rec->pNext) {
        if (rec->wType != DNS_TYPE_PTR || rec->Data.PTR.pNameHost == nullptr) {
            continue;
        }

        QString fullInstance = QString::fromLatin1(rec->Data.PTR.pNameHost).trimmed();
        if (fullInstance.endsWith('.')) {
            fullInstance.chop(1);
        }

        const QString suffix = "." + kServiceType + ".local";
        QString name = fullInstance;
        if (name.endsWith(suffix, Qt::CaseInsensitive)) {
            name.chop(suffix.size());
        }

        if (name.isEmpty()) {
            continue;
        }

        CameraInfo info;
        info.name = name;
        cameraMap[name] = info;
        LogManager::log(QString("Found camera service: %1").arg(name));
    }

    DnsRecordListFree(browseResults, DnsFreeRecordList);

    for (auto it = cameraMap.begin(); it != cameraMap.end(); ++it) {
        QString instanceFqdn = QString("%1.%2.local").arg(it.key(), kServiceType);
        DNS_RECORD* serviceResults = nullptr;
        QByteArray instanceFqdnBytes = instanceFqdn.toLatin1();
        DNS_STATUS serviceStatus = DnsQuery_A(
            instanceFqdnBytes.constData(),
            DNS_TYPE_ANY,
            DNS_QUERY_MULTICAST_ONLY,
            nullptr,
            &serviceResults,
            nullptr);

        if (serviceStatus != ERROR_SUCCESS) {
            continue;
        }

        for (DNS_RECORD* rec = serviceResults; rec; rec = rec->pNext) {
            if (rec->wType == DNS_TYPE_SRV && rec->Data.SRV.pNameTarget != nullptr) {
                it->port = static_cast<int>(rec->Data.SRV.wPort);
                it->hostname = QString::fromLatin1(rec->Data.SRV.pNameTarget);
                if (it->hostname.endsWith('.')) {
                    it->hostname.chop(1);
                }
            } else if (rec->wType == DNS_TYPE_TEXT && rec->Data.TXT.dwStringCount > 0) {
                QStringList txtEntries;
                for (DWORD i = 0; i < rec->Data.TXT.dwStringCount; ++i) {
                    if (rec->Data.TXT.pStringArray[i] != nullptr) {
                        txtEntries << QString::fromLatin1(rec->Data.TXT.pStringArray[i]);
                    }
                }
                parseTxtRecord(txtEntries.join(' '), *it);
            }
        }

        DnsRecordListFree(serviceResults, DnsFreeRecordList);

        if (!it->hostname.isEmpty()) {
            DNS_RECORD* hostResults = nullptr;
            QByteArray hostnameBytes = it->hostname.toLatin1();
            DNS_STATUS hostStatus = DnsQuery_A(
                hostnameBytes.constData(),
                DNS_TYPE_A,
                DNS_QUERY_MULTICAST_ONLY,
                nullptr,
                &hostResults,
                nullptr);

            if (hostStatus == ERROR_SUCCESS) {
                for (DNS_RECORD* rec = hostResults; rec; rec = rec->pNext) {
                    if (rec->wType == DNS_TYPE_A) {
                        IN_ADDR addr;
                        addr.S_un.S_addr = rec->Data.A.IpAddress;
                        QString ip = QString::fromLatin1(inet_ntoa(addr));
                        if (!ip.isEmpty()) {
                            it->ip = ip;
                            break;
                        }
                    }
                }
                DnsRecordListFree(hostResults, DnsFreeRecordList);
            }
        }

        if (it->ip.isEmpty() && !it->hostname.isEmpty()) {
            QHostInfo hostInfo = QHostInfo::fromName(it->hostname);
            if (hostInfo.error() == QHostInfo::NoError && !hostInfo.addresses().isEmpty()) {
                for (const QHostAddress &addr : hostInfo.addresses()) {
                    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                        it->ip = addr.toString();
                        break;
                    }
                }
            }
        }
    }

    m_cameras.clear();
    for (const CameraInfo &info : cameraMap) {
        if (!info.ip.isEmpty() && info.port > 0) {
            m_cameras.append(info);
        }
    }

    updateDiscoveredCamerasList();
    return true;
}
#endif

void MdnsManager::parseDiscoveryOutput(const QString &output)
{
    // Parse dns-sd -Z output format on macOS:
    // _f1sh-camera._tcp                               PTR     F1sh Camera TX._f1sh-camera._tcp
    // F1sh Camera TX._f1sh-camera._tcp                SRV     0 0 8888 s4v-cam2-1.local.
    // F1sh Camera TX._f1sh-camera._tcp                TXT     "protocol=udp" "encoding=h264" "control_port=50051"
    // s4v-cam2-1.local.                               A       192.168.3.132
    // s4v-cam2-1.local.                               AAAA    fe80::fdc7:793:e4c1:e78e

    QStringList lines = output.split('\n');

    // First pass: find all PTR records (camera names)
    QMap<QString, CameraInfo> cameraMap;

    for (const QString &line : lines) {
        // Match PTR record: servicetype PTR instancename.servicetype
        QRegularExpression ptrRegex(R"(_f1sh-camera\._tcp\s+PTR\s+(.+)\._f1sh-camera\._tcp)");
        QRegularExpressionMatch ptrMatch = ptrRegex.match(line);
        if (ptrMatch.hasMatch()) {
            QString name = ptrMatch.captured(1).trimmed();
            CameraInfo info;
            info.name = name;
            cameraMap[name] = info;
            LogManager::log(QString("Found camera service: %1").arg(name));
        }
    }

    // Second pass: find SRV records (hostname and port)
    for (const QString &line : lines) {
        for (auto it = cameraMap.begin(); it != cameraMap.end(); ++it) {
            QString name = it.key();
            // Match SRV record: instancename.servicetype SRV priority weight port target
            QString pattern = QString(R"(%1\._f1sh-camera\._tcp\s+SRV\s+\d+\s+\d+\s+(\d+)\s+(\S+))").arg(QRegularExpression::escape(name));
            QRegularExpression srvRegex(pattern);
            QRegularExpressionMatch srvMatch = srvRegex.match(line);
            if (srvMatch.hasMatch()) {
                it->port = srvMatch.captured(1).toInt();
                it->hostname = srvMatch.captured(2);
                // Remove trailing dot from hostname
                if (it->hostname.endsWith('.')) {
                    it->hostname.chop(1);
                }
                LogManager::log(QString("  Service %1: port %2, host %3").arg(name).arg(it->port).arg(it->hostname));
            }
        }
    }

    // Third pass: find TXT records
    for (const QString &line : lines) {
        for (auto it = cameraMap.begin(); it != cameraMap.end(); ++it) {
            QString name = it.key();
            QString pattern = QString(R"(%1\._f1sh-camera\._tcp\s+TXT\s+(.+))").arg(QRegularExpression::escape(name));
            QRegularExpression txtRegex(pattern);
            QRegularExpressionMatch txtMatch = txtRegex.match(line);
            if (txtMatch.hasMatch()) {
                QString txtData = txtMatch.captured(1);
                // Remove quotes and parse
                txtData.replace("\"", " ");
                parseTxtRecord(txtData, *it);
                LogManager::log(QString("  TXT: protocol=%1, encoding=%2, control_port=%3")
                    .arg(it->protocol, it->encoding).arg(it->controlPort));
            }
        }
    }

    // Fourth pass: find A records (IP addresses)
    for (const QString &line : lines) {
        for (auto it = cameraMap.begin(); it != cameraMap.end(); ++it) {
            if (it->hostname.isEmpty()) continue;

            // Match A record: hostname A ipaddress
            QString pattern = QString(R"(%1\.?\s+A\s+(\d+\.\d+\.\d+\.\d+))").arg(QRegularExpression::escape(it->hostname));
            QRegularExpression aRegex(pattern);
            QRegularExpressionMatch aMatch = aRegex.match(line);
            if (aMatch.hasMatch()) {
                it->ip = aMatch.captured(1);
                LogManager::log(QString("  IP for %1: %2").arg(it->hostname, it->ip));
            }
        }
    }

    // If we didn't find A records, try to resolve hostnames
    for (auto it = cameraMap.begin(); it != cameraMap.end(); ++it) {
        if (it->ip.isEmpty() && !it->hostname.isEmpty()) {
            LogManager::log(QString("Resolving hostname: %1").arg(it->hostname));
            QHostInfo hostInfo = QHostInfo::fromName(it->hostname);
            if (hostInfo.error() == QHostInfo::NoError && !hostInfo.addresses().isEmpty()) {
                for (const QHostAddress &addr : hostInfo.addresses()) {
                    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                        it->ip = addr.toString();
                        LogManager::log(QString("  Resolved to: %1").arg(it->ip));
                        break;
                    }
                }
            }
        }
    }

    // Add valid cameras to the list
    for (const CameraInfo &info : cameraMap) {
        if (!info.ip.isEmpty() && info.port > 0) {
            m_cameras.append(info);
        }
    }

    updateDiscoveredCamerasList();
}

void MdnsManager::updateDiscoveredCamerasList()
{
    m_discoveredCameras.clear();
    for (const CameraInfo &info : m_cameras) {
        QVariantMap map;
        map["name"] = info.name;
        map["hostname"] = info.hostname;
        map["ip"] = info.ip;
        map["port"] = info.port;
        map["controlPort"] = info.controlPort;
        map["protocol"] = info.protocol;
        map["encoding"] = info.encoding;
        m_discoveredCameras.append(map);
    }
    emit discoveredCamerasChanged();
    emit cameraCountChanged();
}

void MdnsManager::setCameraIp(const QString &ip)
{
    if (m_cameraIp != ip) {
        m_cameraIp = ip;
        emit cameraIpChanged();
    }
}

void MdnsManager::setCameraHostname(const QString &hostname)
{
    if (m_cameraHostname != hostname) {
        m_cameraHostname = hostname;
        emit cameraHostnameChanged();
    }
}

void MdnsManager::setCameraPort(int port)
{
    if (m_cameraPort != port) {
        m_cameraPort = port;
        emit cameraPortChanged();
    }
}

void MdnsManager::setControlPort(int port)
{
    if (m_controlPort != port) {
        m_controlPort = port;
        emit controlPortChanged();
    }
}

void MdnsManager::setProtocol(const QString &protocol)
{
    if (m_protocol != protocol) {
        m_protocol = protocol;
        emit protocolChanged();
    }
}

void MdnsManager::setEncoding(const QString &encoding)
{
    if (m_encoding != encoding) {
        m_encoding = encoding;
        emit encodingChanged();
    }
}

void MdnsManager::setIsDiscovering(bool discovering)
{
    if (m_isDiscovering != discovering) {
        m_isDiscovering = discovering;
        emit isDiscoveringChanged();
    }
}

void MdnsManager::setCameraFound(bool found)
{
    if (m_cameraFound != found) {
        m_cameraFound = found;
        emit cameraFoundChanged();
    }
}
