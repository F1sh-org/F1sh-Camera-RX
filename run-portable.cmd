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
set "GST_PLUGIN_PATH=%BASE%lib\gstreamer-1.0"
set "GST_PLUGIN_SYSTEM_PATH_1_0=%GST_PLUGIN_PATH%"
set "GST_PLUGIN_SCANNER=%BASE%libexec\gstreamer-1.0\gst-plugin-scanner.exe"
set "GSETTINGS_SCHEMA_DIR=%BASE%share\glib-2.0\schemas"
set "GIO_MODULE_DIR=%BASE%lib\gio\modules"
set "GDK_PIXBUF_MODULEDIR=%BASE%lib\gdk-pixbuf-2.0\2.10.0\loaders"
set "GDK_PIXBUF_MODULE_FILE=%BASE%lib\gdk-pixbuf-2.0\2.10.0\loaders.cache"
set "GTK_DATA_PREFIX=%BASE%"
set "GTK_EXE_PREFIX=%BASE%"
set "GTK_PATH=%BASE%"
set "XDG_DATA_DIRS=%BASE%share"
set "SSL_CERT_FILE=%BASE%etc\ssl\certs\ca-bundle.crt"
"%EXE%" %*
