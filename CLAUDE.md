# F1sh-Camera-RX Project Guide

## Project Overview

**F1sh-Camera-RX** is a cross-platform desktop video receiver application for the F1sh Camera system. It receives and displays real-time H.264 video streams over UDP from a camera transmitter (TX) device.

- **Language**: C (C11 standard)
- **UI Framework**: GTK+ 3.0
- **Build System**: Meson
- **Video Pipeline**: GStreamer 1.0
- **Platforms**: Linux, Windows (MSYS2), macOS
- **Total Code**: ~3,353 lines across 15 source files

## Directory Structure

```
F1sh-Camera-RX/
├── src/                      # Source code (15 files)
│   ├── main.c               # Application entry point (221 lines)
│   ├── f1sh_camera_rx.h     # Main header with data structures
│   ├── stream.c             # GStreamer pipeline management (196 lines)
│   ├── http_client.c        # TX server communication (260 lines)
│   ├── http_server.c        # Local RX HTTP server on port 8889 (174 lines)
│   ├── ui_main.c            # Main UI window (828 lines)
│   ├── ui_configuration.c   # Configuration dialog (402 lines)
│   ├── ui_login.c           # Login dialog (89 lines)
│   ├── ui_rotate.c          # Video rotation controls (140 lines)
│   ├── ui_wifi.c            # WiFi configuration UI (167 lines)
│   ├── ui_log.c             # Logging system with UI (221 lines)
│   ├── serial_probe.c       # Serial port detection (208 lines)
│   ├── serial_service.c/.h  # Serial communication
│   └── wifi_info.c          # WiFi/network info - Windows only (159 lines)
├── scripts/windows/         # Windows packaging scripts
│   ├── package-msys2-portable.ps1
│   ├── build-installer.ps1
│   └── f1sh-camera-rx-installer.iss
├── docs/
│   └── PACKAGING.md         # Windows packaging documentation
├── .github/
│   ├── workflows/
│   │   └── commit-check.yml # CI workflow
│   └── copilot-instructions.md
├── meson.build              # Build configuration
├── native-file.ini          # Windows build configuration
├── run-portable.cmd         # Windows portable launcher
└── README.md                # User documentation
```

## Core Architecture

### Data Structures (f1sh_camera_rx.h)

#### Config
Stream configuration settings:
- `tx_host`: TX server IP (default: 127.0.0.1)
- `tx_http_port`: TX HTTP API port (default: 8888)
- `rx_host`: RX host IP for receiving stream (default: 127.0.0.1)
- `rx_port`: UDP stream port (default: 5000)
- `width`: Video width (default: 1280)
- `height`: Video height (default: 720)
- `framerate`: FPS (default: 30)
- `rotation`: 0-3 for none, 90°, 180°, 270° (default: 0)

#### App
Global application state containing:
- Configuration
- GTK widget pointers for all UI elements
- GStreamer pipeline and bus watch
- cURL handle for HTTP communication
- Connection and streaming status flags
- Serial port information

## Key Components

### 1. Main Entry Point ([main.c](src/main.c))

**Purpose**: Application initialization and main loop

**Key Responsibilities**:
- Sets up platform-specific GStreamer environment
- Configures plugin paths for bundled deployments (macOS .app, Windows portable)
- Initializes GTK, GStreamer, and cURL
- Creates main UI and starts HTTP server on port 8889
- Enters GTK main loop

**Platform-specific Setup**:
- macOS: Detects .app bundles and configures plugin paths
- Windows: Uses portable bundle environment variables
- Linux: Standard system paths

### 2. Video Streaming ([stream.c](src/stream.c))

**Purpose**: GStreamer pipeline management for low-latency video display

**Key Features**:
- Platform-optimized decoders:
  - **macOS**: `vtdec_hw` (VideoToolbox hardware acceleration)
  - **Windows**: `d3d11h264dec + d3d11videosink` (Direct3D 11)
  - **Linux**: `avdec_h264` (software decoder fallback)
- Pipeline structure: `udpsrc → rtph264depay → h264parse → decoder → [videoflip] → sink`
- 50ms max latency for real-time streaming
- Automatic fallback to software decoder if hardware fails
- Supports dynamic rotation via `videoflip` element

**Important Functions**:
- `create_pipeline()`: Creates GStreamer pipeline with platform-specific elements
- `restart_pipeline()`: Stops and recreates pipeline (used for rotation changes)

### 3. HTTP Client ([http_client.c](src/http_client.c))

**Purpose**: Communication with TX server using libcurl and JSON

**Endpoints**:
- `GET /health` - Tests connection to TX server
- `POST /config` - Sends stream configuration (host, port, resolution, framerate)
- `POST /swap` - Request TX to swap rotation
- `POST /noswap` - Request TX to disable rotation swap

**Features**:
- JSON payload generation using jansson
- Retry logic and error handling
- Thread-safe with GTK idle callbacks

### 4. HTTP Server ([http_server.c](src/http_server.c))

**Purpose**: Local HTTP server for external control

**Details**:
- Listens on port 8889
- Runs in separate pthread
- Endpoint: `POST /rotate` - Receives rotation updates from external sources
- Safely updates UI via GTK idle callbacks
- Uses libmicrohttpd

### 5. User Interface Components

#### Main Window ([ui_main.c](src/ui_main.c) - 828 lines)
- Stream status display
- Camera connection status
- WiFi and serial port information
- Integrated log viewer with interactive mode
- Buttons for streaming and configuration access

#### Configuration Dialog ([ui_configuration.c](src/ui_configuration.c) - 402 lines)
- TX/RX IP configuration
- Resolution presets (720p, 1080p)
- Framerate presets (30, 50, 60 fps)
- Rotation controls
- Connection testing
- Login-protected (credentials: admin/steam4vietnam)

#### Rotation Dialog ([ui_rotate.c](src/ui_rotate.c) - 140 lines)
- Radio buttons for 0°, 90°, 180°, 270°
- Live rotation preview with custom drawing
- Restarts stream pipeline when rotation changes

#### Logging System ([ui_log.c](src/ui_log.c) - 221 lines)
- Thread-safe logging with buffer before UI is ready
- Word wrapping at 120 characters
- Interactive mode for text selection
- All operations via `g_idle_add()` for thread safety

#### Login Dialog ([ui_login.c](src/ui_login.c) - 89 lines)
- Simple authentication for advanced settings
- Hard-coded credentials (admin/steam4vietnam)

#### WiFi Configuration ([ui_wifi.c](src/ui_wifi.c) - 167 lines)
- Displays available networks
- Credential input for WiFi setup

### 6. Serial Communication ([serial_probe.c](src/serial_probe.c) - 208 lines)

**Purpose**: Auto-detect camera on serial ports

**Platform-specific Implementations**:
- **Windows**: Scans COM1-COM20
- **macOS**: Globs `/dev/cu.*` and `/dev/tty.*`
- **Linux**: Tries `/dev/ttyUSB*`, `/dev/ttyACM*`, etc.

**Protocol**:
- 115200 baud, 8N1 configuration
- Probe: sends `{"status":1}\n`, expects same response
- Runs in background thread
- Updates UI via idle callbacks

### 7. WiFi Information ([wifi_info.c](src/wifi_info.c) - 159 lines)

**Windows only** - Uses WLAN API and netsh:
- Retrieves current WiFi SSID
- Gets WiFi password
- Fetches network adapter information
- Stub implementations for other platforms

## Application Workflow

### Startup Sequence
1. `main()` initializes libraries (GTK, GStreamer, cURL)
2. Sets up platform-specific GStreamer plugin paths
3. Creates `App` structure with default configuration
4. Launches main UI window
5. Starts local HTTP server on port 8889
6. Background thread begins serial port probing

### Streaming Workflow
1. User clicks "Open Stream" or "Start Stream"
2. Rotation dialog appears (optional: select rotation angle)
3. HTTP client sends configuration to TX server via `POST /config`
4. GStreamer pipeline starts listening on UDP port
5. TX server begins streaming H.264 video over UDP
6. Video displays in auto-created window
7. Rotation changes trigger pipeline restart

## Dependencies

### Core Libraries
- **GTK+ 3.0** - GUI framework
- **GStreamer 1.0** - Video pipeline
  - gstreamer-video-1.0 for video overlay
- **libcurl** - HTTP client
- **jansson** - JSON parsing/generation
- **libmicrohttpd** - HTTP server
- **threads** - POSIX threading (C11)

### Platform-specific
- **Windows**: wlanapi, iphlpapi (WiFi/network info)
- **macOS**: mach-o/dyld (bundle path detection)

## Build System

### Meson Configuration
- Project version: 1.0.0
- C11 standard
- Warning level 3
- Windows: `win_subsystem = 'windows'` (no console)

### Build Commands

```bash
# Linux/macOS
meson setup builddir
meson compile -C builddir

# Windows (MSYS2 UCRT64)
meson setup builddir --native-file native-file.ini
meson compile -C builddir
```

## Packaging and Distribution

### Windows Portable Bundle ([scripts/windows/package-msys2-portable.ps1](scripts/windows/package-msys2-portable.ps1))
- Collects executable and all MSYS2 DLLs via ntldd
- Bundles GTK assets (themes, icons, schemas)
- Includes full GStreamer plugin set
- Compiles schemas and loader caches
- Creates self-contained `dist/F1sh-Camera-RX` folder
- Launcher script: [run-portable.cmd](run-portable.cmd)

### Windows Installer ([scripts/windows/build-installer.ps1](scripts/windows/build-installer.ps1))
- Uses Inno Setup 6
- Installs to Program Files
- Creates Start Menu and desktop shortcuts
- Registers uninstaller
- Version extracted from meson build info

See [docs/PACKAGING.md](docs/PACKAGING.md) for detailed instructions.

## Default Configuration

| Setting | Default Value |
|---------|--------------|
| TX Server | 127.0.0.1:8888 (HTTP API) |
| RX Host | 127.0.0.1:5000 (UDP stream) |
| RX HTTP Server | Port 8889 |
| Resolution | 1280x720 |
| Framerate | 30 fps |
| Rotation | 0° (none) |
| Serial Baud | 115200, 8N1 |
| Admin Credentials | admin/steam4vietnam |

## Technical Highlights

### Thread Safety
- All GTK UI operations from worker threads use `g_idle_add()`
- Mutex-protected logging buffer
- pthread-based HTTP server isolated from GTK thread

### Memory Management
- GLib allocators (`g_strdup`, `g_free`) for GTK integration
- Proper cleanup in `app_cleanup()` function
- cURL handle reuse with `curl_easy_reset()`

### Error Handling
- Automatic fallback to software decoder if hardware fails
- Connection testing before streaming
- Detailed logging for diagnostics
- HTTP retry logic

### Platform Adaptations
- Bundle-aware plugin discovery for portable deployments
- Platform-specific serial port patterns
- OS-optimized video decoder pipelines

## Common Development Tasks

### Adding a New UI Component
1. Create `ui_<component>.c` in [src/](src/)
2. Add function declarations to [f1sh_camera_rx.h](src/f1sh_camera_rx.h)
3. Add GTK widget pointers to `App` structure if needed
4. Update [meson.build](meson.build) to include new source file
5. Call UI function from appropriate location (usually [ui_main.c](src/ui_main.c))

### Modifying Video Pipeline
1. Edit [stream.c](src/stream.c)
2. Update `create_pipeline()` function
3. Test on all target platforms (hardware acceleration varies)
4. Ensure fallback pipeline works

### Adding HTTP Endpoints
- **TX communication**: Edit [http_client.c](src/http_client.c)
- **RX server**: Edit [http_server.c](src/http_server.c)
- Remember to use `g_idle_add()` for GTK updates from HTTP threads

### Changing Default Configuration
- Edit `App` initialization in [main.c](src/main.c)
- Update CLAUDE.md documentation

## CI/CD

**GitHub Actions** ([.github/workflows/commit-check.yml](.github/workflows/commit-check.yml)):
- Runs on pull requests to main
- Validates commit messages
- Checks branch names
- Verifies author information
- Posts results as PR comments

## Key Features Summary

1. **Cross-platform**: Linux, Windows, macOS support
2. **Low-latency streaming**: 50ms target with hardware acceleration
3. **Hardware acceleration**: Platform-specific video decoders
4. **Video rotation**: 0°, 90°, 180°, 270° with live preview
5. **Auto-discovery**: Serial port scanning for camera detection
6. **Remote control**: HTTP API for external rotation control
7. **Flexible deployment**: Portable bundles and installers
8. **WiFi integration**: Windows WiFi credential retrieval
9. **Comprehensive logging**: Thread-safe, buffered log system
10. **Configuration presets**: Common resolutions and framerates

## Debugging Tips

### Viewing Logs
- Enable interactive mode in log viewer for text selection
- Logs show GStreamer pipeline status and HTTP communication

### Testing Connection
- Use "Test Connection" button in configuration dialog
- Verifies TX server is reachable before streaming

### GStreamer Issues
- Check GST_DEBUG environment variable for verbose output
- Verify plugin paths in portable deployments
- Test hardware decoder availability (`gst-inspect-1.0`)

### Serial Port Issues
- Check permissions on Linux/macOS (`/dev/tty*` access)
- Verify correct baud rate (115200)
- Monitor probe log messages for detection attempts
