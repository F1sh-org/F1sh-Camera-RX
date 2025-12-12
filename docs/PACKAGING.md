# Windows Packaging

This project ships as a fully self-contained bundle so that end users do not have to install MSYS2 or any development tools. Packaging happens in two layers:

1. **Portable folder** – a self-contained collection of DLLs, GTK assets, and the full GStreamer plugin directory.
2. **Installer** – an Inno Setup wrapper that copies the portable folder into `Program Files`, registers an uninstaller entry, and drops shortcuts.

Both steps are scripted from PowerShell and can be run from a standard Windows terminal as long as MSYS2 is installed locally for dependency harvesting.

## Prerequisites

- MSYS2 with the UCRT64 environment (`C:\msys64` by default).
- Build-time packages (install from an MSYS2 *UCRT64* shell):
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
    mingw-w64-ucrt-x86_64-ntldd
  ```
- Inno Setup 6 (standard Windows installer) for building the final installer: <https://jrsoftware.org/isinfo.php>

> The scripts assume MSYS2 lives under `C:\msys64`. Pass `-MsysRoot` if you installed it elsewhere.

## Portable bundle (`package-msys2-portable.ps1`)

```
pwsh -File scripts\windows\package-msys2-portable.ps1
```

What the script does:

- Optionally rebuilds (`meson compile -C builddir`) unless you pass `-SkipBuild`.
- Copies `f1sh-camera-rx.exe` into `dist/F1sh-Camera-RX/bin`.
- Uses `ntldd.exe -R` to follow DLL dependencies and copies only what ships with MSYS2 UCRT64 (no system DLLs).
- Copies the GTK runtime assets (themes, icons, schemas, gdk-pixbuf loaders, gio modules, fonts, etc.) and refreshes the caches via `glib-compile-schemas`, `gdk-pixbuf-query-loaders`, and `gio-querymodules`.
- Copies the entire `lib\gstreamer-1.0` directory from MSYS2 so every plugin ships with the bundle (avoids surprises when pipelines evolve).
- Copies Adwaita icons/themes, GTK config, Fontconfig, and Pango data when present and emits warnings if they are missing instead of aborting the build.
- Copies `gst-plugin-scanner.exe` plus shared GStreamer presets.
- Bundles the MSYS2 CA bundle into `etc\ssl\certs` and updates `run-portable.cmd` so libcurl can trust HTTPS endpoints without touching the system trust store.

Running the script creates/refreshes `dist/F1sh-Camera-RX` with the following structure:

```
dist/F1sh-Camera-RX/
  bin/f1sh-camera-rx.exe
  lib/gstreamer-1.0/*.dll (full plugin set)
  libexec/gstreamer-1.0/gst-plugin-scanner.exe
  share/(glib-2.0, gstreamer-1.0, icons, themes)
  etc/(gtk-3.0, fonts, pango, ssl)
  run-portable.cmd (launches with all required env vars)
```

You can zip this folder and hand it directly to users who just want a portable build.

## Installer (`build-installer.ps1` + `.iss`)

```
pwsh -File scripts\windows\build-installer.ps1
```

What happens:

1. Calls `package-msys2-portable.ps1` (respecting `-SkipBuild` if you pass it) to make sure the portable folder is current.
2. Reads the project version from `builddir/meson-info/intro-projectinfo.json`.
3. Invokes `ISCC.exe` (Inno Setup 6) with `scripts/windows/f1sh-camera-rx-installer.iss`, piping the version, source, and output directories as defines.
4. Drops `F1sh-Camera-RX-Setup-<version>.exe` inside `dist/installer`.

The installer:

- Copies the staged folder into `{autopf}\F1sh Camera RX`.
- Registers an entry in **Apps & Features** with an uninstaller.
- Adds a Start Menu shortcut and (optionally) a desktop shortcut pointing to `run-portable.cmd`.
- Offers a "Launch now" checkbox on the final wizard page.

## Troubleshooting tips

- **Missing DLL errors:** rerun `package-msys2-portable.ps1` without `-SkipBuild`. The script copies every dependency that `ntldd -R` reports, so make sure the binary and plugins were compiled against MSYS2 UCRT64 (not MSYS or MINGW32).
- **New GStreamer packages:** rerun the packaging script after installing additional `gst-*` packages so the mirrored `lib\gstreamer-1.0` directory stays in sync.
- **Optional GTK data missing:** warnings about Adwaita, fonts, or Pango simply mean those MSYS2 packages are absent. Install `mingw-w64-ucrt-x86_64-adwaita-icon-theme`, `mingw-w64-ucrt-x86_64-fontconfig`, or `mingw-w64-ucrt-x86_64-pango` (already implied via GTK) if you want them included.
- **Custom MSYS2 path:** `-MsysRoot "D:\\tools\\msys64"` works for both scripts.
- **CI usage:** call the PowerShell scripts from a Windows job and cache the MSYS2 installation to keep build times short.
