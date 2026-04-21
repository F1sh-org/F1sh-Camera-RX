# AGENTS.md - src runtime core
[Scope: Applies to `src/` and its subdirectories]

## OVERVIEW
Native runtime managers (Qt QObject + worker-thread pattern) that bridge QML UI to streaming, serial, WiFi, mDNS, and gRPC.

## WHERE TO LOOK
| Task | Location | Notes |
|---|---|---|
| Startup + context binding | `main_qml.cpp` | Registers all managers and image provider into QML |
| Stream issues/latency/decoder | `streammanager.cpp` | Pipeline assembly, decoder detection, appsink callbacks |
| TX config/load/save flow | `configmanager.cpp` | Settings + HTTP + serial command fallback |
| Camera COM detection | `serialportmanager.cpp` | Cross-platform port filtering and status probe |
| WiFi scan/connect state flow | `wifimanager.cpp` | Serial protocol status 21/22/23 handling |
| mDNS camera discovery | `mdnsmanager.cpp` | TXT parsing + camera selection behavior |
| gRPC TX control plane | `grpcmanager.cpp` | Worker-thread RPC health/config/swap operations |
| Logging behavior | `logmanager.cpp` | Shared logging API and C wrappers |

## CONVENTIONS (src-specific)
- Keep network/serial/gRPC operations off UI thread: use worker object + dedicated `QThread` + signal handoff back to manager.
- Expose QML-facing state as `Q_PROPERTY` and emit `...Changed()` only on actual state transitions.
- Preserve manager object names and semantic contracts used by QML (`configManager`, `streamManager`, `wifiManager`, `mdnsManager`, `grpcManager`, `serialPortManager`, `logManager`).
- In stream pipeline edits, maintain fallback chain from hardware decoders to software decoder.

## ANTI-PATTERNS
- Do not perform blocking I/O directly in manager methods called from QML.
- Do not bypass status logging on protocol transitions; logs are primary field-debug signal.
- Do not change serial JSON status codes without aligning camera firmware protocol expectations.
- Do not modify generated protobuf/grpc files under `builddir/`; update `proto/f1sh_camera.proto` instead.

## HOTSPOTS
- `configmanager.cpp` (~780 LOC): mixed concerns (settings + HTTP + serial + IP detection), high regression surface.
- `streammanager.cpp` (~600 LOC): low-latency pipeline behavior + cross-platform decode paths.
- `mdnsmanager.cpp` (~540 LOC): platform-specific discovery and parser fragility.

## QUICK CHECKS
```bash
meson compile -C builddir
./builddir/f1sh-camera-rx
```
