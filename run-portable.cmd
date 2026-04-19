@echo off
setlocal
set "BASE=%~dp0"
set "BIN=%BASE%bin"
set "EXE=%BIN%\f1sh-camera-rx.exe"
if not exist "%EXE%" (
  echo f1sh-camera-rx.exe not found under %BASE%bin
  exit /b 1
)
set "PATH=%BIN%;%BASE%lib;%PATH%"
set "QT_PLUGIN_PATH=%BASE%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%BASE%platforms"
set "QML2_IMPORT_PATH=%BASE%qml"
set "GST_PLUGIN_PATH=%BASE%lib\gstreamer-1.0"
set "GST_PLUGIN_SYSTEM_PATH_1_0=%GST_PLUGIN_PATH%"
set "GST_PLUGIN_SCANNER=%BASE%libexec\gstreamer-1.0\gst-plugin-scanner.exe"
set "GSETTINGS_SCHEMA_DIR=%BASE%share\glib-2.0\schemas"
set "XDG_DATA_DIRS=%BASE%share"
set "SSL_CERT_FILE=%BASE%etc\ssl\certs\ca-bundle.crt"
"%EXE%" %*
