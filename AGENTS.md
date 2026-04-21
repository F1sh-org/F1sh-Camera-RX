# PROJECT KNOWLEDGE BASE

**Generated:** 2026-04-21 (Asia/Saigon)  
**Commit:** fa803b1  
**Branch:** main

## OVERVIEW
F1sh-Camera-RX is now a Qt6/QML + C++17 desktop receiver, not the older GTK/C layout described in legacy docs. Runtime combines QML UI, GStreamer video ingest, serial/WiFi control, mDNS discovery, and gRPC control-plane integration.

## STRUCTURE
```text
F1sh-Camera-RX/
├── src/                 # Native runtime managers and app entrypoint
├── qml/                 # QML UI modules + assets loaded via qml.qrc
├── proto/               # gRPC/Protobuf contract (codegen source of truth)
├── scripts/windows/     # Portable/installer packaging and post-install bundling
├── docs/                # Packaging guides (new + deprecated legacy)
├── .github/workflows/   # PR policy checks (commit metadata checks)
├── meson.build          # Build graph, codegen targets, Windows install hook
└── qml.qrc              # Resource manifest for runtime QML/assets
```

## WHERE TO LOOK
| Task | Location | Notes |
|---|---|---|
| App startup wiring | `src/main_qml.cpp` | Registers all managers into QML context; loads `qrc:/qml/main.qml` |
| Build/deps/codegen | `meson.build` | Protobuf+gRPC generation, Qt preprocess/resources, Windows install script hook |
| Video pipeline/decoder fallback | `src/streammanager.cpp` | GStreamer pipeline string + appsink frame path |
| Device discovery | `src/mdnsmanager.cpp` | mDNS browse/parse, multi-camera selection |
| Camera serial probing | `src/serialportmanager.cpp` | USB serial filtering + probe protocol |
| WiFi config via camera | `src/wifimanager.cpp` | Serial status protocol (status 21/22/23 flow) |
| TX config/control plane | `src/grpcmanager.cpp`, `proto/f1sh_camera.proto` | gRPC health/config/swap operations |
| Windows packaging | `scripts/windows/meson-post-install.py` | Selective plugin bundling, ntldd DLL harvesting, windeployqt |
| Portable runtime env | `run-portable.cmd` | Required env vars for bundled plugins/QML/certs |

## CODE MAP
| Symbol | Type | Location | Role |
|---|---|---|---|
| `main` | Function | `src/main_qml.cpp` | Application bootstrap and manager registration |
| `ConfigManager` | Class | `src/configmanager.h` | QML-facing config state + worker orchestration |
| `StreamManager` | Class | `src/streammanager.h` | GStreamer lifecycle + frame bridge to QML |
| `GrpcManager` | Class | `src/grpcmanager.h` | Async gRPC operations on worker thread |
| `SerialPortManager` | Class | `src/serialportmanager.h` | Camera serial detection and state signals |
| `WifiManager` | Class | `src/wifimanager.h` | Camera WiFi scan/connect state machine |
| `MdnsManager` | Class | `src/mdnsmanager.h` | mDNS discovery and camera selection |
| `LogManager` | Class | `src/logmanager.h` | Central logging API (C++ + C wrappers) |

## CONVENTIONS
- Treat `meson.build` as architecture/build source of truth when docs disagree.
- All hardware/network I/O runs through worker threads + Qt signals/slots; keep UI thread free of blocking calls.
- QML consumes manager instances via context properties (`configManager`, `streamManager`, `grpcManager`, etc.); preserve names unless updating both C++ registration and QML usage.
- Windows packaging path is Meson-integrated (`meson install` → `meson-post-install.py`), not standalone manual DLL copying.

## ANTI-PATTERNS (THIS PROJECT)
- Do not edit generated files in `builddir/` (`DO NOT EDIT`/`Do not edit by hand`).
- Do not treat legacy packaging docs/scripts as default workflow (`docs/PACKAGING.md` and old scripts are deprecated paths).
- Do not assume GTK/C module layout from old documentation; current codebase is Qt/QML + C++ managers.
- Do not bypass launcher environment for portable Windows builds; use `run-portable.cmd` to set plugin/schema/cert paths.

## UNIQUE STYLES
- Hybrid native stack: Qt/QML frontend + C GStreamer API + gRPC C++ + serial protocol workers.
- Decoder strategy is runtime-detected and platform-prioritized (D3D12/D3D11/NVDEC/QSV/FFmpeg/OpenH264 on Windows).
- QML resources are bundled through `qml.qrc`; asset/module references assume qrc runtime layout.

## COMMANDS
```bash
# Build
meson setup builddir
meson compile -C builddir

# Run (local)
./builddir/f1sh-camera-rx

# Windows packaging
pwsh -File scripts/windows/build-portable-meson.ps1
pwsh -File scripts/windows/build-installer-meson.ps1
```

## NOTES
- CI currently enforces commit/branch/author policy (`.github/workflows/commit-check.yml`); no repository unit-test suite was found.
- `docs/MESON-PACKAGING.md` still contains some GTK-era wording; prefer script/source behavior when resolving conflicts.
- The curated plugin list in `scripts/windows/meson-post-install.py` must track decoder/pipeline changes in `src/streammanager.cpp`.
