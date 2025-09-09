#!/bin/zsh
set -euo pipefail

# Inputs
BUILD_DIR=${1:-"../../builddir"}
DIST_DIR=${2:-"../../dist"}
APP_NAME="f1sh-camera-rx.app"

# Paths
SRC_APP="$BUILD_DIR/$APP_NAME"
OUT_APP="$DIST_DIR/$APP_NAME"
MACOS_DIR="$OUT_APP/Contents/MacOS"
FRAMEWORKS_DIR="$OUT_APP/Contents/Frameworks"
RESOURCES_DIR="$OUT_APP/Contents/Resources"
BIN_PATH="$MACOS_DIR/f1sh-camera-rx"

# Tools check
for t in otool install_name_tool rsync; do
  if ! command -v $t >/dev/null 2>&1; then
    echo "Missing tool: $t" >&2
    exit 1
  fi
done

mkdir -p "$DIST_DIR"

# Copy fresh app bundle
rm -rf "$OUT_APP"
rsync -a "$SRC_APP" "$DIST_DIR/"

mkdir -p "$FRAMEWORKS_DIR" "$RESOURCES_DIR/gstreamer-1.0" "$RESOURCES_DIR"

# Detect GStreamer plugin and scanner source paths
function try_path() { [[ -e "$1" ]] && echo "$1" && return 0 || return 1 }

BREW_PREFIX=""
if command -v brew >/dev/null 2>&1; then
  BREW_PREFIX=$(brew --prefix || true)
fi

CAND_PLUGINS=(
  "$BREW_PREFIX/lib/gstreamer-1.0"
  "/opt/homebrew/lib/gstreamer-1.0"
  "/usr/local/lib/gstreamer-1.0"
)
CAND_SCANNER=(
  "$BREW_PREFIX/libexec/gstreamer-1.0/gst-plugin-scanner"
  "/opt/homebrew/libexec/gstreamer-1.0/gst-plugin-scanner"
  "/usr/local/libexec/gstreamer-1.0/gst-plugin-scanner"
)

PLUG_SRC=""
for p in $CAND_PLUGINS; do
  if [[ -d "$p" ]]; then PLUG_SRC="$p"; break; fi
done

SCANNER_SRC=""
for s in $CAND_SCANNER; do
  if [[ -x "$s" ]]; then SCANNER_SRC="$s"; break; fi
done

if [[ -n "$PLUG_SRC" ]]; then
  echo "Copying GStreamer plugins from: $PLUG_SRC"
  rsync -a --include '*/' --include '*.so' --exclude '*' "$PLUG_SRC/" "$RESOURCES_DIR/gstreamer-1.0/"
else
  echo "Warning: GStreamer plugins not found; app will rely on system plugins"
fi

if [[ -n "$SCANNER_SRC" ]]; then
  echo "Copying gst-plugin-scanner"
  rsync -a "$SCANNER_SRC" "$RESOURCES_DIR/"
  chmod +x "$RESOURCES_DIR/gst-plugin-scanner"
else
  echo "Warning: gst-plugin-scanner not found; plugin discovery may fail"
fi

# Fixup dylib dependencies by copying Homebrew libs into Frameworks and rewriting install names
typeset -A VISITED

function is_system_lib() {
  local path="$1"
  [[ "$path" == /usr/lib/* || "$path" == /System/* ]] && return 0 || return 1
}

function copy_and_rewrite() {
  local target="$1"  # file whose deps we rewrite
  local deps
  deps=($(otool -L "$target" | tail -n +2 | awk '{print $1}'))

  for dep in $deps; do
    # Skip rpath/loader paths and system libs
    if [[ "$dep" == @rpath/* || "$dep" == @loader_path/* || "$dep" == @executable_path/* ]]; then
      continue
    fi
    if is_system_lib "$dep"; then
      continue
    fi
    # Only consider Homebrew or local libs
    if [[ "$dep" == /opt/homebrew/* || "$dep" == /usr/local/* || "$dep" == $BREW_PREFIX/* ]]; then
      local base
      base=$(basename "$dep")
      local dst="$FRAMEWORKS_DIR/$base"
      if [[ ! -e "$dst" ]]; then
        echo "Copying $dep -> $dst"
        rsync -a "$dep" "$dst"
        chmod 755 "$dst" || true
        # Set its own id to @rpath/base
        install_name_tool -id "@rpath/$base" "$dst"
        # Recurse: rewrite deps of this dylib as well
        copy_and_rewrite "$dst"
      fi
      # Point target's reference to @rpath/base
      install_name_tool -change "$dep" "@rpath/$base" "$target"
    fi
  done
}

# Ensure app binary has rpath to Frameworks
if ! otool -l "$BIN_PATH" | grep -q "@executable_path/../Frameworks"; then
  echo "Adding rpath to app binary"
  install_name_tool -add_rpath "@executable_path/../Frameworks" "$BIN_PATH" || true
fi

# Rewrite app binary and any bundled helper binaries
copy_and_rewrite "$BIN_PATH"

# Also rewrite any copied libs to ensure their internal deps point to @rpath
for lib in "$FRAMEWORKS_DIR"/*.dylib(N) "$FRAMEWORKS_DIR"/*.so(N); do
  [[ -e "$lib" ]] || continue
  copy_and_rewrite "$lib"
done

echo "Portable macOS app created at: $OUT_APP"
