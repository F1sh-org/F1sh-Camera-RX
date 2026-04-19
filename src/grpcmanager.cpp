#include "grpcmanager.h"
#include "logmanager.h"

#include <QDebug>

#include <grpcpp/grpcpp.h>
#include "f1sh_camera.grpc.pb.h"

// ============ GrpcWorker Implementation ============

GrpcWorker::GrpcWorker(QObject *parent)
    : QObject(parent)
{
}

GrpcWorker::~GrpcWorker()
{
}

std::shared_ptr<grpc::Channel> GrpcWorker::createChannel(const QString &serverAddress)
{
    return grpc::CreateChannel(serverAddress.toStdString(), grpc::InsecureChannelCredentials());
}

void GrpcWorker::doHealthCheck(const QString &serverAddress)
{
    LogManager::log(QString("gRPC: Health check to %1").arg(serverAddress));

    auto channel = createChannel(serverAddress);
    auto stub = f1sh_camera::F1shCameraService::NewStub(channel);

    f1sh_camera::HealthRequest request;
    f1sh_camera::HealthResponse response;
    grpc::ClientContext context;

    // Set deadline for the RPC
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    grpc::Status status = stub->Health(&context, request, &response);

    if (status.ok()) {
        QString statusStr = QString::fromStdString(response.status());
        LogManager::log(QString("gRPC: Health check successful: %1").arg(statusStr));
        emit healthCheckFinished(statusStr == "healthy", statusStr);
    } else {
        QString errorMsg = QString::fromStdString(status.error_message());
        LogManager::log(QString("gRPC: Health check failed: %1").arg(errorMsg));
        emit healthCheckFinished(false, errorMsg);
    }
}

void GrpcWorker::doGetConfig(const QString &serverAddress)
{
    LogManager::log(QString("gRPC: Getting config from %1").arg(serverAddress));

    auto channel = createChannel(serverAddress);
    auto stub = f1sh_camera::F1shCameraService::NewStub(channel);

    f1sh_camera::GetConfigRequest request;
    f1sh_camera::GetConfigResponse response;
    grpc::ClientContext context;

    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    grpc::Status status = stub->GetConfig(&context, request, &response);

    if (status.ok()) {
        const auto& config = response.config();
        QString host = QString::fromStdString(config.host());
        int port = config.port();
        QString cameraName = QString::fromStdString(config.camera_name());
        QString encoderType = QString::fromStdString(config.encoder_type());
        int width = config.width();
        int height = config.height();
        int framerate = config.framerate();

        LogManager::log(QString("gRPC: Config received - host=%1, port=%2, %3x%4@%5fps")
                        .arg(host).arg(port).arg(width).arg(height).arg(framerate));

        emit getConfigFinished(true, host, port, cameraName, encoderType, width, height, framerate);
    } else {
        QString errorMsg = QString::fromStdString(status.error_message());
        LogManager::log(QString("gRPC: GetConfig failed: %1").arg(errorMsg));
        emit getConfigFinished(false, QString(), 0, QString(), QString(), 0, 0, 0);
    }
}

void GrpcWorker::doUpdateConfig(const QString &serverAddress, const QString &host, int port,
                                 int width, int height, int framerate)
{
    LogManager::log(QString("gRPC: Updating config on %1").arg(serverAddress));

    auto channel = createChannel(serverAddress);
    auto stub = f1sh_camera::F1shCameraService::NewStub(channel);

    f1sh_camera::UpdateConfigRequest request;
    f1sh_camera::UpdateConfigResponse response;
    grpc::ClientContext context;

    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));

    // Set optional fields
    if (!host.isEmpty()) {
        request.set_host(host.toStdString());
    }
    if (port > 0) {
        request.set_port(port);
    }
    if (width > 0) {
        request.set_width(width);
    }
    if (height > 0) {
        request.set_height(height);
    }
    if (framerate > 0) {
        request.set_framerate(framerate);
    }

    grpc::Status status = stub->UpdateConfig(&context, request, &response);

    if (status.ok()) {
        QString message = QString::fromStdString(response.message());
        const auto& config = response.config();

        LogManager::log(QString("gRPC: UpdateConfig result: success=%1, message=%2")
                        .arg(response.success()).arg(message));

        emit updateConfigFinished(response.success(), message,
                                  QString::fromStdString(config.host()),
                                  config.port(), config.width(), config.height(), config.framerate());
    } else {
        QString errorMsg = QString::fromStdString(status.error_message());
        LogManager::log(QString("gRPC: UpdateConfig failed: %1").arg(errorMsg));
        emit updateConfigFinished(false, errorMsg, QString(), 0, 0, 0, 0);
    }
}

void GrpcWorker::doUpdateHost(const QString &serverAddress, const QString &host)
{
    LogManager::log(QString("gRPC: Updating host to %1 on %2").arg(host, serverAddress));

    auto channel = createChannel(serverAddress);
    auto stub = f1sh_camera::F1shCameraService::NewStub(channel);

    f1sh_camera::UpdateHostRequest request;
    f1sh_camera::UpdateHostResponse response;
    grpc::ClientContext context;

    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

    request.set_host(host.toStdString());

    grpc::Status status = stub->UpdateHost(&context, request, &response);

    if (status.ok()) {
        QString message = QString::fromStdString(response.message());
        LogManager::log(QString("gRPC: UpdateHost result: success=%1, message=%2")
                        .arg(response.success()).arg(message));
        emit updateHostFinished(response.success(), message);
    } else {
        QString errorMsg = QString::fromStdString(status.error_message());
        LogManager::log(QString("gRPC: UpdateHost failed: %1").arg(errorMsg));
        emit updateHostFinished(false, errorMsg);
    }
}

void GrpcWorker::doSwapResolution(const QString &serverAddress, int swap)
{
    LogManager::log(QString("gRPC: SwapResolution (swap=%1) on %2").arg(swap).arg(serverAddress));

    auto channel = createChannel(serverAddress);
    auto stub = f1sh_camera::F1shCameraService::NewStub(channel);

    f1sh_camera::SwapResolutionRequest request;
    f1sh_camera::SwapResolutionResponse response;
    grpc::ClientContext context;

    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));

    request.set_swap(swap);

    grpc::Status status = stub->SwapResolution(&context, request, &response);

    if (status.ok()) {
        QString message = QString::fromStdString(response.message());
        const auto& config = response.config();

        LogManager::log(QString("gRPC: SwapResolution result: success=%1, message=%2, new size=%3x%4")
                        .arg(response.success()).arg(message).arg(config.width()).arg(config.height()));

        emit swapResolutionFinished(response.success(), message, config.width(), config.height());
    } else {
        QString errorMsg = QString::fromStdString(status.error_message());
        LogManager::log(QString("gRPC: SwapResolution failed: %1").arg(errorMsg));
        emit swapResolutionFinished(false, errorMsg, 0, 0);
    }
}

// ============ GrpcManager Implementation ============

GrpcManager::GrpcManager(QObject *parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_worker(new GrpcWorker())
{
    // Move worker to background thread
    m_worker->moveToThread(m_workerThread);

    // Connect signals for health check
    connect(this, &GrpcManager::startHealthCheck, m_worker, &GrpcWorker::doHealthCheck);
    connect(m_worker, &GrpcWorker::healthCheckFinished, this, &GrpcManager::onHealthCheckFinished);

    // Connect signals for get config
    connect(this, &GrpcManager::startGetConfig, m_worker, &GrpcWorker::doGetConfig);
    connect(m_worker, &GrpcWorker::getConfigFinished, this, &GrpcManager::onGetConfigFinished);

    // Connect signals for update config
    connect(this, &GrpcManager::startUpdateConfig, m_worker, &GrpcWorker::doUpdateConfig);
    connect(m_worker, &GrpcWorker::updateConfigFinished, this, &GrpcManager::onUpdateConfigFinished);

    // Connect signals for update host
    connect(this, &GrpcManager::startUpdateHost, m_worker, &GrpcWorker::doUpdateHost);
    connect(m_worker, &GrpcWorker::updateHostFinished, this, &GrpcManager::onUpdateHostFinished);

    // Connect signals for swap resolution
    connect(this, &GrpcManager::startSwapResolution, m_worker, &GrpcWorker::doSwapResolution);
    connect(m_worker, &GrpcWorker::swapResolutionFinished, this, &GrpcManager::onSwapResolutionFinished);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // Start worker thread
    m_workerThread->start();
}

GrpcManager::~GrpcManager()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

void GrpcManager::setServerAddress(const QString &address)
{
    if (m_serverAddress != address) {
        m_serverAddress = address;
        emit serverAddressChanged();
    }
}

void GrpcManager::setIsBusy(bool busy)
{
    if (m_isBusy != busy) {
        m_isBusy = busy;
        emit isBusyChanged();
    }
}

void GrpcManager::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void GrpcManager::healthCheck()
{
    if (m_isBusy || m_serverAddress.isEmpty()) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Checking connection...");
    emit startHealthCheck(m_serverAddress);
}

void GrpcManager::onHealthCheckFinished(bool success, const QString &status)
{
    if (m_isConnected != success) {
        m_isConnected = success;
        emit isConnectedChanged();
    }

    if (success) {
        setStatusMessage(QString("Connected: %1").arg(status));
    } else {
        setStatusMessage(QString("Connection failed: %1").arg(status));
    }

    setIsBusy(false);
    emit healthCheckResult(success);
}

void GrpcManager::getConfig()
{
    if (m_isBusy || m_serverAddress.isEmpty()) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Getting configuration...");
    emit startGetConfig(m_serverAddress);
}

void GrpcManager::onGetConfigFinished(bool success, const QString &host, int port,
                                       const QString &cameraName, const QString &encoderType,
                                       int width, int height, int framerate)
{
    if (success) {
        m_txHost = host;
        m_txPort = port;
        m_cameraName = cameraName;
        m_encoderType = encoderType;
        m_txWidth = width;
        m_txHeight = height;
        m_txFramerate = framerate;
        emit configChanged();
        setStatusMessage("Configuration loaded");
    } else {
        setStatusMessage("Failed to get configuration");
    }

    setIsBusy(false);
    emit getConfigResult(success);
}

void GrpcManager::updateConfig(const QString &host, int port, int width, int height, int framerate)
{
    if (m_isBusy || m_serverAddress.isEmpty()) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Updating configuration...");
    emit startUpdateConfig(m_serverAddress, host, port, width, height, framerate);
}

void GrpcManager::onUpdateConfigFinished(bool success, const QString &message,
                                          const QString &host, int port, int width, int height, int framerate)
{
    if (success) {
        m_txHost = host;
        m_txPort = port;
        m_txWidth = width;
        m_txHeight = height;
        m_txFramerate = framerate;
        emit configChanged();
    }

    setStatusMessage(message);
    setIsBusy(false);
    emit updateConfigResult(success, message);
}

void GrpcManager::updateHost(const QString &host)
{
    if (m_isBusy || m_serverAddress.isEmpty()) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Updating host...");
    emit startUpdateHost(m_serverAddress, host);
}

void GrpcManager::onUpdateHostFinished(bool success, const QString &message)
{
    setStatusMessage(message);
    setIsBusy(false);
    emit updateHostResult(success, message);
}

void GrpcManager::swapResolution(int swap)
{
    if (m_isBusy || m_serverAddress.isEmpty()) {
        return;
    }

    setIsBusy(true);
    setStatusMessage("Swapping resolution...");
    emit startSwapResolution(m_serverAddress, swap);
}

void GrpcManager::onSwapResolutionFinished(bool success, const QString &message, int width, int height)
{
    if (success && width > 0 && height > 0) {
        m_txWidth = width;
        m_txHeight = height;
        emit configChanged();
    }

    setStatusMessage(message);
    setIsBusy(false);
    emit swapResolutionResult(success, message, width, height);
}
