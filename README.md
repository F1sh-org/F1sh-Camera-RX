# F1sh Camera RX

A clean and simple H.264 video stream receiver for the F1sh Camera system.

## Features

- H.264 UDP stream reception via GStreamer
- Real-time video display

## Requirements

- GStreamer 1.0
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
   pacman -S mingw-w64-ucrt-x86_64-qt6-base
   pacman -S mingw-w64-ucrt-x86_64-gstreamer
   pacman -S mingw-w64-ucrt-x86_64-gst-plugins-base
   pacman -S mingw-w64-ucrt-x86_64-gst-plugins-good
   pacman -S mingw-w64-ucrt-x86_64-gst-plugins-bad
   pacman -S mingw-w64-ucrt-x86_64-jansson
   pacman -S mingw-w64-ucrt-x86_64-meson
   pacman -S mingw-w64-ucrt-x86_64-cmake
   ```

3. **Build the project**:
   ```bash
   meson setup builddir
   meson compile -C builddir
   ```

4. **Run with correct DLLs on PATH**:
  - Launch from the same UCRT64 shell you built with, so PATH includes MSYS2 DLLs.
  - Or bundle required `*.dll` from `C:\msys64\ucrt64\bin` alongside `f1sh-camera-rx.exe`.


## Running

```bash
./builddir/f1sh-camera-rx
```

## Packaging

### Windows

**New Meson-integrated approach (Recommended):**
```powershell
# Build portable bundle
pwsh -File scripts\windows\build-portable-meson.ps1

# Build installer
pwsh -File scripts\windows\build-installer-meson.ps1
```

See [`docs/MESON-PACKAGING.md`](docs/MESON-PACKAGING.md) for the new Meson-integrated packaging system with selective GStreamer plugin bundling (60-70% smaller bundles).

**Legacy approach:** See [`docs/PACKAGING.md`](docs/PACKAGING.md) for the old manual PowerShell scripts (deprecated).
