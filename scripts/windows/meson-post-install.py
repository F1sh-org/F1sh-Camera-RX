#!/usr/bin/env python3
"""
Meson post-install script for Windows packaging.
Collects dependencies and creates a portable Windows bundle.
"""

import os
import sys
import shutil
import subprocess
import json
from pathlib import Path

def print_step(msg):
    """Print a visible step message."""
    print(f"\n==> {msg}")

def find_msys2_root():
    """Locate MSYS2 installation directory."""
    candidates = [
        os.environ.get('MSYS2_ROOT'),
        r'C:\msys64',
        r'D:\msys64',
    ]

    for candidate in candidates:
        if candidate and os.path.isdir(candidate):
            ucrt64 = os.path.join(candidate, 'ucrt64')
            if os.path.isdir(ucrt64):
                return candidate

    raise RuntimeError(
        "MSYS2 not found. Set MSYS2_ROOT environment variable or "
        "install MSYS2 to C:\\msys64"
    )

def convert_msys_path(path, msys_root):
    """Convert MSYS path to Windows path."""
    if not path:
        return None

    path = path.strip()

    # Already a Windows path
    if ':' in path and len(path) > 2:
        return path.replace('/', '\\')

    # MSYS path like /ucrt64/bin/foo.dll
    if path.startswith('/'):
        if len(path) >= 3 and path[2] == '/':
            # /c/path format
            drive = path[1].upper()
            rest = path[3:].replace('/', '\\')
            return f"{drive}:\\{rest}"
        # Relative to MSYS root
        return os.path.join(msys_root, path[1:].replace('/', '\\'))

    return None

def copy_dll_dependencies(binary_path, dest_bin_dir, ucrt64_root, ntldd_exe, copied_dlls):
    """
    Recursively copy DLL dependencies using ntldd.
    Only copies DLLs from MSYS2 UCRT64, not system DLLs.
    """
    msys_root = os.path.dirname(ucrt64_root)

    try:
        output = subprocess.check_output(
            [ntldd_exe, '-R', binary_path],
            stderr=subprocess.STDOUT,
            text=True
        )
    except subprocess.CalledProcessError as e:
        print(f"Warning: ntldd failed for {binary_path}: {e}")
        return

    for line in output.splitlines():
        # Look for => /path/to/dll.dll
        if '=>' in line:
            parts = line.split('=>')
            if len(parts) >= 2:
                dll_path = convert_msys_path(parts[1].strip().split()[0], msys_root)
                if dll_path and os.path.exists(dll_path):
                    if dll_path.lower().startswith(ucrt64_root.lower()):
                        dll_name = os.path.basename(dll_path)
                        if dll_name not in copied_dlls:
                            shutil.copy2(dll_path, os.path.join(dest_bin_dir, dll_name))
                            copied_dlls.add(dll_name)
                            print(f"  Copied: {dll_name}")

def copy_gstreamer_plugins(dest_plugin_dir, ucrt64_root, ntldd_exe, copied_dlls, dest_bin_dir):
    """
    Copy only the required GStreamer plugins instead of all plugins.
    This significantly reduces bundle size.
    """
    source_plugin_dir = os.path.join(ucrt64_root, 'lib', 'gstreamer-1.0')

    if not os.path.isdir(source_plugin_dir):
        raise RuntimeError(f"GStreamer plugin directory not found: {source_plugin_dir}")

    # Required plugins for the application (based on stream.c analysis)
    required_plugins = {
        # Core elements (always needed)
        'libgstcoreelements.dll',       # queue, capsfilter

        # RTP/UDP streaming (gst-plugins-good)
        'libgstudp.dll',                # udpsrc
        'libgstrtp.dll',                # rtph264depay
        'libgstrtpmanager.dll',         # RTP session management

        # Video parsing (gst-plugins-bad)
        'libgstvideoparsersbad.dll',    # h264parse

        # Windows Direct3D 11 (gst-plugins-bad)
        'libgstd3d11.dll',              # d3d11h264dec, d3d11videosink, d3d11convert, d3d11download

        # Video processing (gst-plugins-base and good)
        'libgstvideoconvertscale.dll',  # videoconvert, videoscale
        'libgstvideofilter.dll',        # videoflip

        # Auto elements (gst-plugins-base)
        'libgstautodetect.dll',         # autovideosink (fallback)

        # Playback elements (gst-plugins-base)
        'libgstplayback.dll',           # playbin (might be useful)

        # Type finding
        'libgsttypefindfunctions.dll',  # Type detection

        # Fallback software decoder (gst-libav)
        'libgstlibav.dll',              # avdec_h264
    }

    print_step(f"Copying {len(required_plugins)} required GStreamer plugins")

    copied_count = 0
    for plugin_name in required_plugins:
        plugin_path = os.path.join(source_plugin_dir, plugin_name)
        if os.path.exists(plugin_path):
            dest_path = os.path.join(dest_plugin_dir, plugin_name)
            shutil.copy2(plugin_path, dest_path)
            print(f"  + {plugin_name}")

            # Copy plugin's DLL dependencies
            copy_dll_dependencies(plugin_path, dest_bin_dir, ucrt64_root, ntldd_exe, copied_dlls)
            copied_count += 1
        else:
            print(f"  ! Warning: Plugin not found: {plugin_name}")

    print(f"  Copied {copied_count}/{len(required_plugins)} plugins")

def copy_gtk_assets(dest_share_dir, dest_lib_dir, dest_etc_dir, ucrt64_root):
    """Copy GTK runtime assets (themes, icons, schemas)."""
    print_step("Copying GTK assets")

    # GLib schemas (required)
    schema_src = os.path.join(ucrt64_root, 'share', 'glib-2.0', 'schemas')
    schema_dest = os.path.join(dest_share_dir, 'glib-2.0', 'schemas')
    os.makedirs(schema_dest, exist_ok=True)
    if os.path.isdir(schema_src):
        shutil.copytree(schema_src, schema_dest, dirs_exist_ok=True)
        print("  + GLib schemas")

    # Adwaita icon theme (optional but recommended)
    icon_src = os.path.join(ucrt64_root, 'share', 'icons', 'Adwaita')
    icon_dest = os.path.join(dest_share_dir, 'icons', 'Adwaita')
    if os.path.isdir(icon_src):
        shutil.copytree(icon_src, icon_dest, dirs_exist_ok=True)
        print("  + Adwaita icons")

    # GTK themes (optional)
    theme_src = os.path.join(ucrt64_root, 'share', 'themes', 'Adwaita')
    theme_dest = os.path.join(dest_share_dir, 'themes', 'Adwaita')
    if os.path.isdir(theme_src):
        shutil.copytree(theme_src, theme_dest, dirs_exist_ok=True)
        print("  + Adwaita theme")

    # GStreamer presets
    gst_share_src = os.path.join(ucrt64_root, 'share', 'gstreamer-1.0')
    gst_share_dest = os.path.join(dest_share_dir, 'gstreamer-1.0')
    if os.path.isdir(gst_share_src):
        shutil.copytree(gst_share_src, gst_share_dest, dirs_exist_ok=True)
        print("  + GStreamer presets")

    # GTK runtime libraries
    gtk3_src = os.path.join(ucrt64_root, 'lib', 'gtk-3.0')
    gtk3_dest = os.path.join(dest_lib_dir, 'gtk-3.0')
    if os.path.isdir(gtk3_src):
        shutil.copytree(gtk3_src, gtk3_dest, dirs_exist_ok=True)
        print("  + GTK 3.0 modules")

    # GIO modules
    gio_src = os.path.join(ucrt64_root, 'lib', 'gio', 'modules')
    gio_dest = os.path.join(dest_lib_dir, 'gio', 'modules')
    os.makedirs(gio_dest, exist_ok=True)
    if os.path.isdir(gio_src):
        for item in os.listdir(gio_src):
            shutil.copy2(os.path.join(gio_src, item), os.path.join(gio_dest, item))
        print("  + GIO modules")

    # GDK Pixbuf loaders
    pixbuf_src = os.path.join(ucrt64_root, 'lib', 'gdk-pixbuf-2.0', '2.10.0', 'loaders')
    pixbuf_dest = os.path.join(dest_lib_dir, 'gdk-pixbuf-2.0', '2.10.0', 'loaders')
    os.makedirs(pixbuf_dest, exist_ok=True)
    if os.path.isdir(pixbuf_src):
        for item in os.listdir(pixbuf_src):
            shutil.copy2(os.path.join(pixbuf_src, item), os.path.join(pixbuf_dest, item))
        print("  + GDK Pixbuf loaders")

    # Optional config directories
    for subdir in ['gtk-3.0', 'fonts', 'pango']:
        etc_src = os.path.join(ucrt64_root, 'etc', subdir)
        etc_dest = os.path.join(dest_etc_dir, subdir)
        if os.path.isdir(etc_src):
            shutil.copytree(etc_src, etc_dest, dirs_exist_ok=True)
            print(f"  + {subdir} config")

    # SSL certificates
    ca_bundle_src = os.path.join(ucrt64_root, 'ssl', 'certs', 'ca-bundle.crt')
    ca_bundle_dest = os.path.join(dest_etc_dir, 'ssl', 'certs', 'ca-bundle.crt')
    if os.path.exists(ca_bundle_src):
        os.makedirs(os.path.dirname(ca_bundle_dest), exist_ok=True)
        shutil.copy2(ca_bundle_src, ca_bundle_dest)
        print("  + SSL certificates")

def compile_schemas_and_caches(dest_dir, ucrt64_root):
    """Compile GLib schemas and regenerate loader caches."""
    print_step("Compiling schemas and caches")

    # Compile GLib schemas
    glib_compile = os.path.join(ucrt64_root, 'bin', 'glib-compile-schemas.exe')
    schema_dir = os.path.join(dest_dir, 'share', 'glib-2.0', 'schemas')
    if os.path.exists(glib_compile) and os.path.isdir(schema_dir):
        subprocess.run([glib_compile, schema_dir], check=True)
        print("  Compiled GLib schemas")

    # Query GIO modules
    gio_query = os.path.join(ucrt64_root, 'bin', 'gio-querymodules.exe')
    gio_dir = os.path.join(dest_dir, 'lib', 'gio', 'modules')
    if os.path.exists(gio_query) and os.path.isdir(gio_dir):
        subprocess.run([gio_query, gio_dir], check=True, stdout=subprocess.DEVNULL)
        print("  Queried GIO modules")

    # Query GDK Pixbuf loaders
    pixbuf_query = os.path.join(ucrt64_root, 'bin', 'gdk-pixbuf-query-loaders.exe')
    pixbuf_dir = os.path.join(dest_dir, 'lib', 'gdk-pixbuf-2.0', '2.10.0', 'loaders')
    if os.path.exists(pixbuf_query) and os.path.isdir(pixbuf_dir):
        env = os.environ.copy()
        env['GDK_PIXBUF_MODULEDIR'] = pixbuf_dir
        result = subprocess.run(
            [pixbuf_query],
            capture_output=True,
            text=True,
            env=env,
            check=True
        )
        cache_file = os.path.join(dest_dir, 'lib', 'gdk-pixbuf-2.0', '2.10.0', 'loaders.cache')
        with open(cache_file, 'w', encoding='utf-8') as f:
            f.write(result.stdout)
        print("  Generated Pixbuf loader cache")

def main():
    """Main packaging function."""
    if len(sys.argv) < 2:
        print("Usage: meson-post-install.py <install-destdir>")
        sys.exit(1)

    destdir = Path(sys.argv[1]).resolve()

    print_step(f"Starting Windows packaging for: {destdir}")

    # Find MSYS2
    msys_root = find_msys2_root()
    ucrt64_root = os.path.join(msys_root, 'ucrt64')
    print(f"Using MSYS2 UCRT64: {ucrt64_root}")

    # Find ntldd
    ntldd_exe = os.path.join(ucrt64_root, 'bin', 'ntldd.exe')
    if not os.path.exists(ntldd_exe):
        raise RuntimeError(
            f"ntldd.exe not found at {ntldd_exe}. "
            "Install: pacman -S mingw-w64-ucrt-x86_64-ntldd"
        )

    # Create directory structure
    dest_bin = destdir / 'bin'
    dest_lib = destdir / 'lib'
    dest_plugin = dest_lib / 'gstreamer-1.0'
    dest_libexec = destdir / 'libexec' / 'gstreamer-1.0'
    dest_share = destdir / 'share'
    dest_etc = destdir / 'etc'

    for d in [dest_bin, dest_lib, dest_plugin, dest_libexec, dest_share, dest_etc]:
        d.mkdir(parents=True, exist_ok=True)

    # Track copied DLLs to avoid duplicates
    copied_dlls = set()

    # Find the main executable
    exe_path = dest_bin / 'f1sh-camera-rx.exe'
    if not exe_path.exists():
        raise RuntimeError(f"Executable not found: {exe_path}")

    # Copy main executable dependencies
    print_step("Copying main executable dependencies")
    copy_dll_dependencies(str(exe_path), str(dest_bin), ucrt64_root, ntldd_exe, copied_dlls)

    # Copy GStreamer plugins (selective)
    copy_gstreamer_plugins(str(dest_plugin), ucrt64_root, ntldd_exe, copied_dlls, str(dest_bin))

    # Copy GTK assets
    copy_gtk_assets(str(dest_share), str(dest_lib), str(dest_etc), ucrt64_root)

    # Copy GStreamer plugin scanner
    scanner_src = os.path.join(ucrt64_root, 'libexec', 'gstreamer-1.0', 'gst-plugin-scanner.exe')
    scanner_dest = dest_libexec / 'gst-plugin-scanner.exe'
    if os.path.exists(scanner_src):
        shutil.copy2(scanner_src, str(scanner_dest))
        print_step("Copied GStreamer plugin scanner")

    # Compile schemas and caches
    compile_schemas_and_caches(str(destdir), ucrt64_root)

    print_step("Packaging complete!")
    print(f"Portable bundle ready at: {destdir}")

if __name__ == '__main__':
    main()
