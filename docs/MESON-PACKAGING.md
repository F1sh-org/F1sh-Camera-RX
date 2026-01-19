# Meson-Based Windows Packaging

This document describes the **new Meson-integrated packaging system** for F1sh-Camera-RX on Windows. This replaces the old manual PowerShell approach with a cleaner, more maintainable build system integration.

## What Changed

### Old Approach (Deprecated)
- Manual PowerShell scripts that copied **all** GStreamer plugins (~100+ DLLs)
- Required running `package-msys2-portable.ps1` separately
- Difficult to maintain and debug
- Resulted in bloated bundles (200+ MB)

### New Approach (Recommended)
- Meson-integrated post-install script
- **Selective plugin copying** - only ~12 required plugins
- Automatic dependency bundling during `meson install`
- Significantly smaller bundle size (~60-80 MB)
- Single command to build and package

## Benefits

✅ **Smaller bundle size** - 60-70% reduction by copying only needed GStreamer plugins
✅ **Faster builds** - Less file copying, parallel operations
✅ **Better maintainability** - Build system integration instead of separate scripts
✅ **Automatic dependency resolution** - Uses ntldd to find only required DLLs
✅ **Standard Meson workflow** - Works like any other Meson project

## Prerequisites

Same as before:

- **MSYS2** with UCRT64 environment (`C:\msys64` by default)
- Required MSYS2 packages (install from UCRT64 shell):

```bash
pacman -S \
  mingw-w64-ucrt-x86_64-meson \
  mingw-w64-ucrt-x86_64-gtk3 \
  mingw-w64-ucrt-x86_64-adwaita-icon-theme \
  mingw-w64-ucrt-x86_64-gstreamer \
  mingw-w64-ucrt-x86_64-gst-plugins-base \
  mingw-w64-ucrt-x86_64-gst-plugins-good \
  mingw-w64-ucrt-x86_64-gst-plugins-bad \
  mingw-w64-ucrt-x86_64-gst-libav \
  mingw-w64-ucrt-x86_64-libcurl \
  mingw-w64-ucrt-x86_64-jansson \
  mingw-w64-ucrt-x86_64-libmicrohttpd \
  mingw-w64-ucrt-x86_64-ntldd \
  mingw-w64-ucrt-x86_64-python
```

- **Inno Setup 6** (for building installers): <https://jrsoftware.org/isinfo.php>
- **Python 3** (usually comes with MSYS2)

## Building a Portable Bundle

### Quick Start

```powershell
# From repository root (Windows PowerShell or CMD)
pwsh -File scripts\windows\build-portable-meson.ps1
```

This single command will:
1. Configure the Meson build (if not already configured)
2. Compile the application
3. Install to `dist/F1sh-Camera-RX`
4. Run the post-install script to bundle dependencies
5. Create a ready-to-distribute portable folder

### Output Structure

```
dist/F1sh-Camera-RX/
├── bin/
│   ├── f1sh-camera-rx.exe          # Main application
│   └── *.dll                        # Runtime dependencies (~30-40 DLLs)
├── lib/
│   ├── gstreamer-1.0/               # Only 12 required plugins
│   │   ├── libgstcoreelements.dll
│   │   ├── libgstd3d11.dll
│   │   ├── libgstlibav.dll
│   │   └── ... (9 more)
│   ├── gtk-3.0/                     # GTK modules
│   ├── gio/modules/                 # GIO modules
│   └── gdk-pixbuf-2.0/              # Pixbuf loaders
├── libexec/gstreamer-1.0/
│   └── gst-plugin-scanner.exe
├── share/
│   ├── glib-2.0/schemas/            # Compiled schemas
│   ├── gstreamer-1.0/               # GStreamer presets
│   ├── icons/Adwaita/               # Icon theme
│   └── themes/Adwaita/              # GTK theme
├── etc/
│   ├── gtk-3.0/                     # GTK config
│   ├── fonts/                       # Fontconfig
│   └── ssl/certs/ca-bundle.crt      # SSL certificates
└── run-portable.cmd                 # Launcher script
```

### Script Options

```powershell
# Skip the build step (if already built)
pwsh -File scripts\windows\build-portable-meson.ps1 -SkipBuild

# Skip Meson configuration (if already configured)
pwsh -File scripts\windows\build-portable-meson.ps1 -SkipConfigure

# Custom build directory
pwsh -File scripts\windows\build-portable-meson.ps1 -BuildDir "C:\custom\build"

# Custom output directory
pwsh -File scripts\windows\build-portable-meson.ps1 -DestDir "C:\custom\output"

# Custom MSYS2 location
pwsh -File scripts\windows\build-portable-meson.ps1 -MsysRoot "D:\msys64"
```

## Building an Installer

```powershell
# From repository root
pwsh -File scripts\windows\build-installer-meson.ps1
```

This will:
1. Call `build-portable-meson.ps1` to create the portable bundle
2. Read the version from Meson build info
3. Compile an Inno Setup installer
4. Output: `dist/installer/F1sh-Camera-RX-Setup-<version>.exe`

## How It Works

### 1. Meson Build Configuration

The [meson.build](../../meson.build) file includes Windows-specific install targets:

```meson
if host_machine.system() == 'windows'
  # Install the portable launcher
  install_data('run-portable.cmd', install_dir : '.')

  # Run post-install script for dependency bundling
  python = find_program('python3', 'python', required : true)
  meson.add_install_script(
    python,
    'scripts/windows/meson-post-install.py',
    meson.global_build_root() / '..' / get_option('prefix')
  )
endif
```

### 2. Post-Install Script

[`scripts/windows/meson-post-install.py`](meson-post-install.py) is automatically executed during `meson install`:

**Key features:**
- Uses `ntldd.exe` to recursively find DLL dependencies
- **Selective GStreamer plugin copying** based on actual pipeline requirements:
  - Core: `libgstcoreelements.dll` (queue, capsfilter)
  - UDP/RTP: `libgstudp.dll`, `libgstrtp.dll`
  - Video parsing: `libgstvideoparsersbad.dll` (h264parse)
  - D3D11 hardware acceleration: `libgstd3d11.dll`
  - Video processing: `libgstvideoconvertscale.dll`, `libgstvideofilter.dll`
  - Fallback: `libgstlibav.dll` (software decoder)
- Filters out system DLLs (only bundles MSYS2 UCRT64 dependencies)
- Copies GTK assets (themes, icons, schemas)
- Compiles GLib schemas and regenerates loader caches

### 3. Required GStreamer Plugins

Based on the video pipeline in [src/stream.c](../../src/stream.c), only these plugins are bundled:

| Plugin DLL | Purpose | Pipeline Elements |
|------------|---------|-------------------|
| `libgstcoreelements.dll` | Core elements | queue, capsfilter |
| `libgstudp.dll` | UDP streaming | udpsrc |
| `libgstrtp.dll` | RTP depayloading | rtph264depay |
| `libgstrtpmanager.dll` | RTP session management | rtpmanager |
| `libgstvideoparsersbad.dll` | Video parsing | h264parse |
| `libgstd3d11.dll` | Direct3D 11 (Windows) | d3d11h264dec, d3d11videosink, d3d11convert, d3d11download |
| `libgstvideoconvertscale.dll` | Video conversion | videoconvert, videoscale |
| `libgstvideofilter.dll` | Video filters | videoflip |
| `libgstautodetect.dll` | Auto elements | autovideosink (fallback) |
| `libgsttypefindfunctions.dll` | Type detection | typefind |
| `libgstlibav.dll` | FFmpeg codecs | avdec_h264 (fallback) |

**Total: ~12 plugins** vs ~100+ in the old approach

## Testing the Bundle

```cmd
cd dist\F1sh-Camera-RX
run-portable.cmd
```

The launcher script sets up all necessary environment variables:
- `GST_PLUGIN_PATH` - Points to bundled GStreamer plugins
- `GSETTINGS_SCHEMA_DIR` - GLib schema directory
- `GDK_PIXBUF_MODULEDIR` - Pixbuf loaders
- `SSL_CERT_FILE` - SSL certificate bundle
- And more...

## Troubleshooting

### Missing DLL errors

If you get "DLL not found" errors at runtime:

1. Ensure all MSYS2 packages are installed
2. Reconfigure and rebuild:
   ```powershell
   Remove-Item builddir -Recurse -Force
   pwsh -File scripts\windows\build-portable-meson.ps1
   ```

### GStreamer plugin not found

If you get "no element named 'xyz'" errors:

1. Check if the plugin is listed in [meson-post-install.py](meson-post-install.py) line ~115 (`required_plugins`)
2. Add the plugin DLL if needed
3. Rebuild

### Python not found

Meson needs Python to run the post-install script:

```bash
# In MSYS2 UCRT64 shell
pacman -S mingw-w64-ucrt-x86_64-python
```

### MSYS2 not detected

Set the `MSYS2_ROOT` environment variable:

```powershell
$env:MSYS2_ROOT = "D:\msys64"
pwsh -File scripts\windows\build-portable-meson.ps1
```

Or pass it as a parameter:

```powershell
pwsh -File scripts\windows\build-portable-meson.ps1 -MsysRoot "D:\msys64"
```

## Migration from Old Scripts

If you're migrating from the old `package-msys2-portable.ps1` approach:

### Old workflow:
```powershell
meson compile -C builddir
pwsh -File scripts\windows\package-msys2-portable.ps1
pwsh -File scripts\windows\build-installer.ps1
```

### New workflow:
```powershell
# Portable bundle
pwsh -File scripts\windows\build-portable-meson.ps1

# Installer
pwsh -File scripts\windows\build-installer-meson.ps1
```

**Note:** The old scripts are still available for compatibility but are **deprecated**.

## CI/CD Integration

For GitHub Actions or other CI systems:

```yaml
- name: Setup MSYS2
  uses: msys2/setup-msys2@v2
  with:
    msystem: UCRT64
    install: >-
      mingw-w64-ucrt-x86_64-meson
      mingw-w64-ucrt-x86_64-gtk3
      mingw-w64-ucrt-x86_64-gstreamer
      mingw-w64-ucrt-x86_64-gst-plugins-base
      mingw-w64-ucrt-x86_64-gst-plugins-good
      mingw-w64-ucrt-x86_64-gst-plugins-bad
      mingw-w64-ucrt-x86_64-gst-libav
      mingw-w64-ucrt-x86_64-libcurl
      mingw-w64-ucrt-x86_64-jansson
      mingw-w64-ucrt-x86_64-libmicrohttpd
      mingw-w64-ucrt-x86_64-ntldd
      mingw-w64-ucrt-x86_64-python

- name: Build portable bundle
  shell: pwsh
  run: |
    pwsh -File scripts\windows\build-portable-meson.ps1

- name: Build installer
  shell: pwsh
  run: |
    pwsh -File scripts\windows\build-installer-meson.ps1

- name: Upload artifacts
  uses: actions/upload-artifact@v3
  with:
    name: F1sh-Camera-RX-Windows
    path: |
      dist/F1sh-Camera-RX/
      dist/installer/*.exe
```

## Further Optimizations

Future improvements could include:

- **Dynamic plugin scanning** - Auto-detect required plugins from pipeline strings
- **WiX Toolset** - Native MSI installers instead of Inno Setup
- **Conan/vcpkg integration** - Centralized dependency management
- **Debug symbol stripping** - Further reduce binary sizes
- **Compression** - 7-Zip portable archives

## Files Reference

| File | Purpose |
|------|---------|
| [meson.build](../../meson.build) | Build configuration with Windows install targets |
| [scripts/windows/meson-post-install.py](meson-post-install.py) | Python post-install script for dependency bundling |
| [scripts/windows/build-portable-meson.ps1](build-portable-meson.ps1) | PowerShell wrapper for Meson install |
| [scripts/windows/build-installer-meson.ps1](build-installer-meson.ps1) | PowerShell script for Inno Setup compilation |
| [scripts/windows/f1sh-camera-rx-installer.iss](f1sh-camera-rx-installer.iss) | Inno Setup configuration |
| [run-portable.cmd](../../run-portable.cmd) | Portable launcher with environment setup |

## Support

For issues with the new packaging system:
- Check [PACKAGING.md](PACKAGING.md) for general packaging info
- Review [meson-post-install.py](meson-post-install.py) for plugin list
- Open an issue with build logs