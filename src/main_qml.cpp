#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQmlContext>
#include <QDebug>
#include <iostream>
#include "serialportmanager.h"
#include "wifimanager.h"
#include "configmanager.h"
#include "logmanager.h"
#include "streammanager.h"

int main(int argc, char *argv[])
{
    // Force console output on Windows
    #ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    #endif
    
    std::cerr << "Starting F1sh Camera RX..." << std::endl;
    
    QApplication app(argc, argv);
    
    // Set the Quick Controls 2 style (optional)
    QQuickStyle::setStyle("Basic");
    
    QQmlApplicationEngine engine;
    
    // Create and register LogManager first (so other managers can use it)
    LogManager logManager;
    engine.rootContext()->setContextProperty("logManager", &logManager);
    
    // Create and register SerialPortManager
    SerialPortManager serialManager;
    engine.rootContext()->setContextProperty("serialPortManager", &serialManager);
    
    // Create and register WifiManager
    WifiManager wifiManager;
    engine.rootContext()->setContextProperty("wifiManager", &wifiManager);
    
    // Create and register ConfigManager
    ConfigManager configManager;
    engine.rootContext()->setContextProperty("configManager", &configManager);

    // Create and register StreamManager for video streaming
    StreamManager streamManager;
    engine.rootContext()->setContextProperty("streamManager", &streamManager);

    // Register image provider for video frames
    engine.addImageProvider("videoframe", streamManager.imageProvider());
    
    // Connect SerialPortManager to WifiManager - update serial port when camera connects
    QObject::connect(&serialManager, &SerialPortManager::connectedPortChanged, [&]() {
        wifiManager.setSerialPort(serialManager.connectedPort());
        configManager.setSerialPort(serialManager.connectedPort());

        // Log camera connection status
        if (serialManager.cameraConnected()) {
            LogManager::log(QString("Camera connected via COM port: %1").arg(serialManager.connectedPort()));
        } else {
            LogManager::log("No camera connected via COM port");
        }
    });

    // Connect WifiManager to SerialPortManager - pause auto-detection during WiFi scan
    QObject::connect(&wifiManager, &WifiManager::isScanningChanged, [&]() {
        if (wifiManager.isScanning()) {
            serialManager.pauseAutoDetect();
        } else {
            serialManager.resumeAutoDetect();
        }
    });
    
    // Add import paths for QML modules
    engine.addImportPath("qrc:/qml");
    
    std::cerr << "Loading QML..." << std::endl;
    
    // Load the main QML file
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            std::cerr << "Failed to load QML!" << std::endl;
            QCoreApplication::exit(-1);
        } else {
            std::cerr << "QML loaded successfully" << std::endl;
        }
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    std::cerr << "Entering event loop..." << std::endl;
    
    return app.exec();
}
