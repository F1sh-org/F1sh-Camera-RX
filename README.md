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
On Windows, use MSYS2:

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
   pacman -S mingw-w64-ucrt-x86_64-cmake
   pacman -S mingw-w64-ucrt-x86_64-libmicrohttpd
   ```

3. **Build the project**:
   ```bash
   meson setup builddir --native-file native-file.ini
   meson compile -C builddir
   ```

4. **Run with correct DLLs on PATH**:
  - Launch from the same UCRT64 shell you built with, so PATH includes MSYS2 DLLs.
  - Or bundle required `*.dll` from `C:\msys64\ucrt64\bin` alongside `f1sh-camera-rx.exe`.

Important: Do not mix MSYS2 packages with the official GStreamer or GTK installers. Mixing runtimes causes CRITICAL GTK warnings, non-responsive buttons, and shutdown hangs due to event loop ABI mismatches.

## Running

```bash
./builddir/f1sh-camera-rx
```
