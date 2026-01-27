# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

F1sh Camera RX is an H.264 video stream receiver for the F1sh Camera system. The project has **transitioned from GTK3/C to Qt6/C++17 with Qt Quick/QML**. The current architecture uses **Qt Quick/QML for UI** with **C++ backend managers** for business logic.

**Current state:** Active Qt6/C++17 application with integrated Qt Quick/QML UI:
- **Main entry point:** [src/main_qml.cpp](src/main_qml.cpp) - Sets up QML engine and registers C++ backend managers
- **QML UI:** Full application in [qml/](qml/) directory
  - Main window: [qml/main.qml](qml/main.qml) - Fullscreen window with popup-based navigation
  - Screens: [qml/content/](qml/content/) - Start, Settings, CameraDisplay, Log, Wifi_pass
  - Resources: [qml.qrc](qml.qrc) - Qt resource file bundling all QML and assets
- **C++ Backend:** Four manager classes exposed to QML as context properties
- **Build system:** Meson with integrated QML resource compilation
- **Legacy code:** Old GTK/C files (`*.c` in `src/`) and Qt Widgets UI (`mainwindow.*`, `ui/mainwindow.ui`) are not compiled

## Build System

This project uses **Meson** as its build system.

### Building and Running

```bash
# Configure build
meson setup builddir

# Compile
meson compile -C builddir

# Run
./builddir/f1sh-camera-rx.exe   # Windows
./builddir/f1sh-camera-rx       # Linux
```

On Windows, ensure you run from MSYS2 UCRT64 shell so dependencies are on PATH.

### Dependencies

**Required:**
- Qt6 (Core, Gui, Widgets, Qml, Quick, QuickControls2, Network)
- GStreamer 1.0 with plugins (base, good, bad) - for future video streaming
- jansson (JSON library)
- Python 3 (for build scripts)
- Windows-specific: wlanapi, iphlpapi, winhttp (linked automatically on Windows)

On Windows/MSYS2 UCRT64:
```bash
pacman -S mingw-w64-ucrt-x86_64-qt6-base \
          mingw-w64-ucrt-x86_64-qt6-declarative \
          mingw-w64-ucrt-x86_64-gstreamer \
          mingw-w64-ucrt-x86_64-gst-plugins-base \
          mingw-w64-ucrt-x86_64-gst-plugins-good \
          mingw-w64-ucrt-x86_64-gst-plugins-bad \
          mingw-w64-ucrt-x86_64-jansson \
          mingw-w64-ucrt-x86_64-meson \
          mingw-w64-ucrt-x86_64-cmake
```

## Architecture

### Overview

The application follows a **QML frontend + C++ backend** pattern. Four C++ manager classes handle business logic and hardware interfacing, exposed to QML as context properties in [src/main_qml.cpp:31-45](src/main_qml.cpp#L31-L45).

### C++ Backend Managers

All managers are QObject subclasses registered as QML context properties:

1. **SerialPortManager** ([src/serialportmanager.h](src/serialportmanager.h))
   - **Purpose:** Auto-detect F1sh Camera via USB serial ports
   - **Pattern:** Main thread QObject + background worker thread (`SerialPortWorker`)
   - **Key methods:** `startAutoDetect()`, `stopAutoDetect()`, `refresh()`
   - **QML properties:** `cameraConnected`, `connectedPort`, `availableCameras`
   - **Note:** Uses Windows-specific serial port enumeration and probing

2. **WifiManager** ([src/wifimanager.h](src/wifimanager.h))
   - **Purpose:** Scan WiFi networks via camera serial commands
   - **Pattern:** Main thread QObject + background worker thread (`WifiWorker`)
   - **Key methods:** `refresh()`, `setSerialPort()`
   - **QML properties:** `networks` (QVariantList), `isScanning`, `serialPort`, `errorMessage`
   - **Communication:** Sends JSON commands to camera over serial port

3. **ConfigManager** ([src/configmanager.h](src/configmanager.h))
   - **Purpose:** Configure camera settings (resolution, framerate, rotation, IP addresses)
   - **Methods:** `testConnection()`, `saveConfig()`, `loadConfigFromCamera()`, `loadSettings()`, `saveSettings()`
   - **QML properties:** `txServerIp`, `rxHostIp`, `resolutionIndex`, `framerateIndex`, `rotate`, `cameraConnected`, `directionSaved`
   - **Communication:** HTTP requests to camera TX server + serial commands
   - **Persistence:** Uses QSettings for saving/loading user configuration

4. **LogManager** ([src/logmanager.h](src/logmanager.h))
   - **Purpose:** Centralized logging system
   - **Pattern:** Singleton with static `log()` method
   - **QML properties:** `logMessages` (QStringList)
   - **Usage:** Other managers call `LogManager::log()` to add timestamped messages

### Manager Connections

In [src/main_qml.cpp:48-58](src/main_qml.cpp#L48-L58), SerialPortManager signals are connected to update WifiManager and ConfigManager with the active serial port when a camera is detected.

### QML UI Structure

- **Main window:** [qml/main.qml](qml/main.qml)
  - Fullscreen frameless window
  - Popup-based navigation (Settings, Log, CameraDisplay)
  - Starts with [qml/content/Start.qml](qml/content/Start.qml) as initial screen

- **Screens:** (in [qml/content/](qml/content/))
  - `Start.qml` - Main menu/home screen
  - `Settings.qml` - Camera configuration (uses ConfigManager)
  - `Wifi_pass.ui.qml` - WiFi network selection/password input (uses WifiManager)
  - `CameraDisplay.qml` - Video stream view (GStreamer integration pending)
  - `Log.qml` - System logs display (uses LogManager)

- **Resource compilation:** [qml.qrc](qml.qrc) bundles all QML files and assets into the executable via Qt's resource system

### QML-C++ Integration

- C++ managers are accessed in QML as global objects (e.g., `serialPortManager.cameraConnected`)
- QML calls C++ methods using `Q_INVOKABLE` (e.g., `configManager.testConnection()`)
- C++ property changes automatically update QML bindings via Qt's property system
- Signal/slot connections enable async operations (e.g., WiFi scanning on background thread)

### Video Streaming (Pending Implementation)

Legacy `stream.c` contains the GStreamer pipeline logic that needs reimplementation:
- UDP source receiving H.264 stream on port 8888
- RTP depayloading → H.264 parsing
- Hardware-accelerated decoding (D3D11 on Windows) with libav fallback
- Video display sink

**Integration point:** [qml/content/CameraDisplay.qml](qml/content/CameraDisplay.qml) will embed GStreamer video output.

### Legacy Code (Not Compiled)

Files in `src/` with `.c` extensions are relics of the old GTK3/C architecture:
- [src/stream.c](src/stream.c), [src/serial_probe.c](src/serial_probe.c), [src/serial_service.c](src/serial_service.c), [src/wifi_info.c](src/wifi_info.c)
- Qt Widgets UI: [src/mainwindow.cpp](src/mainwindow.cpp), [src/mainwindow.h](src/mainwindow.h), [ui/mainwindow.ui](ui/mainwindow.ui)
- Old Qt Design Studio project: [ui/UntitledProject/](ui/UntitledProject/)

Do not modify these files - they are not part of the current build.

## Windows Packaging

```powershell
# Build portable bundle (60-80 MB)
pwsh -File scripts\windows\build-portable-meson.ps1

# Build installer
pwsh -File scripts\windows\build-installer-meson.ps1
```

The [scripts/windows/meson-post-install.py](scripts/windows/meson-post-install.py) script automatically runs during `meson install` to:
- Bundle required DLLs from MSYS2 UCRT64 using `ntldd.exe` for dependency resolution
- Copy only ~12 essential GStreamer plugins (60-70% smaller than bundling all plugins)
- Create `dist/F1sh-Camera-RX/` directory structure

See [docs/MESON-PACKAGING.md](docs/MESON-PACKAGING.md) for detailed documentation.

## Development Workflow

### Adding New C++ Backend Managers

1. Create class files in `src/` (e.g., `newmanager.h`, `newmanager.cpp`)
2. Add to [meson.build:25-31](meson.build#L25-L31) sources list
3. If using Qt signals/slots (Q_OBJECT macro), add header to `moc_headers` in [meson.build:10](meson.build#L10)
4. Register in [src/main_qml.cpp](src/main_qml.cpp) as context property:
   ```cpp
   NewManager newManager;
   engine.rootContext()->setContextProperty("newManager", &newManager);
   ```
5. Access in QML via the registered name (e.g., `newManager.someProperty`)

### Working with QML UI

1. QML files are in [qml/](qml/) and [qml/content/](qml/content/)
2. After adding/removing QML files, update [qml.qrc](qml.qrc) resource file
3. QML changes require full rebuild: `meson compile -C builddir`
4. C++ manager properties/methods are directly accessible in QML (see QML-C++ Integration above)

### Meson Build System Notes

- **MOC processing:** Qt headers with Q_OBJECT are automatically processed by `qt6.preprocess()` in [meson.build:9-12](meson.build#L9-L12)
- **QML resources:** Compiled via `qt6.compile_resources()` in [meson.build:15-17](meson.build#L15-L17)
- **Windows libraries:** wlanapi, iphlpapi, winhttp are automatically linked on Windows builds
- **Reconfigure after meson.build changes:** `meson setup --reconfigure builddir`

## Important Notes

- **Current branch:** `huy/add_new_ui` (main branch for PRs: `main`)
- **Version:** 2.0.0 (defined in [meson.build:2](meson.build#L2))
- **Commit style:** Use conventional commit prefixes (`feat:`, `fix:`, `refactor:`, `chore:`)
- **Platform-specific code:** Windows-only code uses `#ifdef _WIN32` (see manager headers)
- **Threading:** SerialPortManager and WifiManager use worker threads to avoid blocking the UI during I/O operations
- **Legacy code:** Do not modify `.c` files, old Qt Widgets UI, or `ui/UntitledProject/` - not part of active build
