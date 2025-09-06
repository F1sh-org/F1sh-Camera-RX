# F1sh Camera RX

A clean and simple H.264 video stream receiver for the F1sh Camera system.

## Features

- Simple GTK-based user interface
- HTTP API communication with TX server
- H.264 UDP stream reception via GStreamer
- Real-time video display

## Requirements

- GTK+ 3.0
- GStreamer 1.0
- libcurl
- jansson (JSON library)
- Meson build system

## Building

### Linux
```bash
meson setup builddir
meson compile -C builddir
```

### Windows
On Windows, use MSYS2 for all dependencies to ensure compatibility:

1. **Install MSYS2** if not already installed
2. **Install dependencies** in MSYS2 UCRT64 terminal:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gtk3
   pacman -S mingw-w64-ucrt-x86_64-gstreamer
   pacman -S mingw-w64-ucrt-x86_64-gst-plugins-base
   pacman -S mingw-w64-ucrt-x86_64-gst-plugins-good
   pacman -S mingw-w64-ucrt-x86_64-gst-plugins-bad
   pacman -S mingw-w64-ucrt-x86_64-curl
   pacman -S mingw-w64-ucrt-x86_64-jansson
   pacman -S mingw-w64-ucrt-x86_64-meson
   ```

3. **Build the project**:
   ```bash
   meson setup builddir --native-file native-file.ini
   meson compile -C builddir
   ```

**Important**: Do not mix MSYS2 libraries with the official GStreamer installer, as this causes binary compatibility issues that prevent GTK widgets from working correctly.

## Running

```bash
./builddir/f1sh-camera-rx
```

## Usage

1. Enter the TX server IP address and port (default: 127.0.0.1:8888)
2. Configure RX host IP and stream port (where to receive video)
3. Set video parameters (width, height, framerate)
4. Click "Test Connection" to verify TX server is reachable
5. Click "Connect" to configure TX server and start streaming

## Architecture

The application consists of 4 main modules:

- **main.c**: Application initialization and main loop
- **ui.c**: GTK user interface and event handling  
- **http_client.c**: HTTP communication with TX server
- **stream.c**: GStreamer pipeline for video reception

## Configuration

The TX server expects JSON configuration via POST to `/config`:

```json
{
  "host": "192.168.1.77",
  "port": 5000,
  "width": 1280,
  "height": 720,
  "framerate": 30
}
```

The health check is via GET to `/health` expecting:

```json
{
  "status": "healthy"
}
```
