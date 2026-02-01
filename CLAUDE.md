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
- **C++ Backend:** Five manager classes exposed to QML as context properties
- **Build system:** Meson with integrated QML resource compilation

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
- GStreamer 1.0 (gstreamer-1.0, gstreamer-video-1.0, gstreamer-app-1.0)
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
          mingw-w64-ucrt-x86_64-meson \
          mingw-w64-ucrt-x86_64-cmake
```

## Architecture

### Overview

The application follows a **QML frontend + C++ backend** pattern. Five C++ manager classes handle business logic and hardware interfacing, exposed to QML as context properties in [src/main_qml.cpp:33-50](src/main_qml.cpp#L33-L50).

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

5. **StreamManager** ([src/streammanager.h](src/streammanager.h))
   - **Purpose:** GStreamer-based H.264 video stream reception and display
   - **Key methods:** `start()`, `stop()`, `detectDecoders()`, `setPreferredDecoder()`
   - **QML properties:** `isStreaming`, `status`, `currentDecoder`, `port`, `rotate`, `availableDecoders`
   - **Components:**
     - `VideoFrameProvider`: QQuickImageProvider subclass for QML frame display
     - GStreamer pipeline: UDP source → RTP depayload → H.264 decode → AppSink
   - **Image provider:** Registered as `"videoframe"` - access in QML via `"image://videoframe/frame"`
   - **Hardware decoding:** Auto-detects available decoders with priority for hardware acceleration

### Manager Connections

In [src/main_qml.cpp:56-75](src/main_qml.cpp#L56-L75), SerialPortManager signals are connected to update WifiManager and ConfigManager with the active serial port when a camera is detected. WifiManager also signals SerialPortManager to pause auto-detection during WiFi scans.

### QML UI Structure

- **Main window:** [qml/main.qml](qml/main.qml)
  - Fullscreen frameless window
  - Popup-based navigation (Settings, Log, CameraDisplay)
  - Starts with [qml/content/Start.qml](qml/content/Start.qml) as initial screen

- **Screens:** (in [qml/content/](qml/content/))
  - `Start.qml` - Main menu/home screen
  - `Settings.qml` - Camera configuration (uses ConfigManager)
  - `Wifi_pass.ui.qml` - WiFi network selection/password input (uses WifiManager)
  - `CameraDisplay.qml` - Video stream view (uses StreamManager)
  - `Log.qml` - System logs display (uses LogManager)

- **Resource compilation:** [qml.qrc](qml.qrc) bundles all QML files and assets into the executable via Qt's resource system

### QML-C++ Integration

- C++ managers are accessed in QML as global objects (e.g., `serialPortManager.cameraConnected`)
- QML calls C++ methods using `Q_INVOKABLE` (e.g., `configManager.testConnection()`)
- C++ property changes automatically update QML bindings via Qt's property system
- Signal/slot connections enable async operations (e.g., WiFi scanning on background thread)

### Video Streaming

StreamManager implements the GStreamer pipeline for H.264 video reception:
- UDP source receiving RTP/H.264 stream on configurable port (default 8888)
- RTP depayloading → H.264 parsing → hardware/software decoding
- Automatic decoder detection with preference for hardware acceleration (D3D11 on Windows)
- Frame delivery to QML via VideoFrameProvider image provider

**QML integration:** [qml/content/CameraDisplay.qml](qml/content/CameraDisplay.qml) displays frames via the `"image://videoframe/frame"` source.

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
2. Add to [meson.build:30-37](meson.build#L30-L37) sources list
3. If using Qt signals/slots (Q_OBJECT macro), add header to `moc_headers` in [meson.build:15](meson.build#L15)
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

- **MOC processing:** Qt headers with Q_OBJECT are automatically processed by `qt6.preprocess()` in [meson.build:14-17](meson.build#L14-L17)
- **QML resources:** Compiled via `qt6.compile_resources()` in [meson.build:20-22](meson.build#L20-L22)
- **Windows libraries:** wlanapi, iphlpapi, winhttp are automatically linked on Windows builds
- **Reconfigure after meson.build changes:** `meson setup --reconfigure builddir`

## Important Notes

- **Current branch:** `huy/add_new_ui` (main branch for PRs: `main`)
- **Version:** 2.0.0 (defined in [meson.build:2](meson.build#L2))
- **Commit style:** Use conventional commit prefixes (`feat:`, `fix:`, `refactor:`, `chore:`)
- **Platform-specific code:** Windows-only code uses `#ifdef _WIN32` (see manager headers)
- **Threading:** SerialPortManager and WifiManager use worker threads to avoid blocking the UI during I/O operations
