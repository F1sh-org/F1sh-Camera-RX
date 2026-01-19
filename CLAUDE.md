# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

F1sh Camera RX is an H.264 video stream receiver for the F1sh Camera system. The project has **transitioned from a GTK3/C codebase to Qt6/C++** (commit 1110658). The codebase contains legacy C files (`serial_*.c`, `stream.c`, `wifi_info.c`) that reference a non-existent `f1sh_camera_rx.h` header - these are remnants from the old architecture and are not currently compiled.

**Current state:** The active codebase is Qt6/C++17 with **two UI implementations**:

1. **Qt Widgets UI** (minimal, currently built by Meson):
   - [src/mainwindow.cpp](src/mainwindow.cpp), [src/mainwindow.h](src/mainwindow.h), [ui/mainwindow.ui](ui/mainwindow.ui)
   - Simple window with basic controls
   - Built via [meson.build](meson.build)

2. **Qt Quick/QML UI** (new, comprehensive):
   - Full project in [ui/UntitledProject/](ui/UntitledProject/)
   - Created with Qt Design Studio 4.8 (Qt 6.8)
   - Includes login screen ([Screen01.ui.qml](ui/UntitledProject/UntitledProjectContent/Screen01.ui.qml))
   - Standalone QML application with CMake build system
   - **Not yet integrated** with the main Meson build

## Build System

This project uses **Meson** as its primary build system for the Qt Widgets application.

### Building the Qt Widgets Application

```bash
# Configure build
meson setup builddir

# Compile
meson compile -C builddir

# Run
./builddir/f1sh-camera-rx
```

### Building the Qt Quick/QML Application

The Qt Quick UI in [ui/UntitledProject/](ui/UntitledProject/) uses CMake:

```bash
cd ui/UntitledProject
mkdir build && cd build
cmake ..
cmake --build .
```

**Note:** The QML UI is not yet integrated with the main Meson build system.

### Dependencies

**Qt Widgets Application:**
- Qt6 (Core, Gui, Widgets)
- GStreamer 1.0 with plugins (base, good, bad)
- jansson (JSON library)
- Python 3 (for build scripts)

**Qt Quick/QML Application:**
- Qt6 (Core, Gui, Widgets, Quick, Qml)
- Qt Design Studio Components (bundled in [ui/UntitledProject/Dependencies/](ui/UntitledProject/Dependencies/))

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

### Qt Widgets Integration

- **MOC Processing:** Qt's Meta-Object Compiler processes [src/mainwindow.h](src/mainwindow.h) automatically via `qt6.preprocess()` in [meson.build](meson.build)
- **UI Files:** [ui/mainwindow.ui](ui/mainwindow.ui) is processed into `ui_mainwindow.h` at build time
- **Signals/Slots:** Standard Qt signal-slot mechanism (see [src/mainwindow.cpp](src/mainwindow.cpp))

### Qt Quick/QML Integration

- **Main Entry Point:** [ui/UntitledProject/App/main.cpp](ui/UntitledProject/App/main.cpp)
- **QML Engine:** Uses `QQmlApplicationEngine` to load QML components
- **UI Definition:** [ui/UntitledProject/UntitledProjectContent/Screen01.ui.qml](ui/UntitledProject/UntitledProjectContent/Screen01.ui.qml) defines the main login screen
- **Application Structure:** [ui/UntitledProject/UntitledProjectContent/App.qml](ui/UntitledProject/UntitledProjectContent/App.qml) is the root window
- **Design Studio Components:** Custom QML components bundled in [Dependencies/Components/](ui/UntitledProject/Dependencies/Components/) for effects, flow views, and logic helpers

### Video Streaming Architecture (Legacy - To Be Reimplemented)

The old C implementation used GStreamer with this pipeline structure:
- UDP source receiving H.264 stream
- RTP depayloading
- H.264 parsing
- Hardware-accelerated decoding (D3D11 on Windows)
- Video display sink

**Note:** This GStreamer pipeline logic exists in legacy `stream.c` but needs to be reimplemented in the Qt/C++ architecture.

### Legacy Code

Files in `src/` with `.c` extensions are **not compiled** in the current build:
- [src/stream.c](src/stream.c) - GStreamer video pipeline
- [src/serial_probe.c](src/serial_probe.c) - Serial port detection
- [src/serial_service.c](src/serial_service.c) - Serial communication
- [src/wifi_info.c](src/wifi_info.c) - Windows WiFi enumeration

These files reference `f1sh_camera_rx.h` which does not exist. They represent the old GTK3/C architecture before the Qt refactor.

## Windows Packaging

The project uses a **Meson-integrated packaging system** for Windows distribution.

### Quick Commands

```powershell
# Build portable bundle (60-80 MB)
pwsh -File scripts\windows\build-portable-meson.ps1

# Build installer
pwsh -File scripts\windows\build-installer-meson.ps1
```

### How Packaging Works

1. **Selective Plugin Bundling:** Only ~12 required GStreamer plugins are copied (vs ~100+ in old approach), reducing bundle size by 60-70%
2. **Post-Install Script:** [scripts/windows/meson-post-install.py](scripts/windows/meson-post-install.py) automatically runs during `meson install` to bundle dependencies
3. **Dependency Resolution:** Uses `ntldd.exe` to recursively find required DLLs from MSYS2 UCRT64
4. **Output:** Creates `dist/F1sh-Camera-RX/` with proper directory structure for portable distribution

See [docs/MESON-PACKAGING.md](docs/MESON-PACKAGING.md) for detailed packaging documentation.

### Required GStreamer Plugins

Based on video pipeline requirements (from legacy `stream.c`):
- Core: `libgstcoreelements.dll`
- Network: `libgstudp.dll`, `libgstrtp.dll`, `libgstrtpmanager.dll`
- Parsing: `libgstvideoparsersbad.dll`
- Decoding: `libgstd3d11.dll` (hardware), `libgstlibav.dll` (fallback)
- Processing: `libgstvideoconvertscale.dll`, `libgstvideofilter.dll`
- Utilities: `libgstautodetect.dll`, `libgsttypefindfunctions.dll`

## Development Workflow

### Working with Qt Widgets UI

1. Edit [ui/mainwindow.ui](ui/mainwindow.ui) in Qt Designer or manually
2. Add signal/slot connections in [src/mainwindow.cpp](src/mainwindow.cpp)
3. Meson automatically handles MOC and UI compilation

### Working with Qt Quick/QML UI

1. Open [ui/UntitledProject/UntitledProject.qmlproject](ui/UntitledProject/UntitledProject.qmlproject) in Qt Design Studio
2. Edit UI files (`.ui.qml` files) in Design Studio's visual editor
3. Modify logic files (`.qml` files) for behavior and state management
4. Build using the CMake workflow (see Build System section)

**Note:** The QML project is currently standalone. Integration with the main application and GStreamer pipeline is pending.

### Adding New Source Files

Update [meson.build](meson.build):
```meson
sources = [
  'src/main.cpp',
  'src/mainwindow.cpp',
  'src/newfile.cpp',  # Add here
]
```

For Qt classes with Q_OBJECT macro, add to `moc_headers`:
```meson
processed = qt6.preprocess(
  moc_headers: ['src/mainwindow.h', 'src/newclass.h'],
  ui_files: ['ui/mainwindow.ui'],
  dependencies: qt6_dep
)
```

## Important Notes

- **Current branch:** `huy/add_new_ui` (main branch for PRs: `main`)
- **Version:** 2.0.0 (defined in [meson.build](meson.build))
- **Platform-specific code:** Use Qt's cross-platform APIs instead of `#ifdef _WIN32` where possible
- **Legacy C code:** Do not modify `.c` files in `src/` - they are not part of the current build
- **GStreamer integration:** Will need to be reimplemented using Qt's multimedia framework or direct GStreamer C++ bindings

## Git Workflow

Recent commits show the progression:
- `01f1c90` - Merged `release/v2.0.0` into `huy/add_new_ui`
- `64686f7` - Added CLAUDE.md documentation
- `480d50c` - Added new UI implementation
- `1110658` - Major refactor from GTK/C to Qt/C++
- `fad66f3` - Re-staged Meson build configuration for Qt

When making commits, follow the existing style: use conventional commit prefixes (`feat:`, `fix:`, `refactor:`, `chore:`).
