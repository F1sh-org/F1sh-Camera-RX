# AGENTS.md - qml UI layer
[Scope: Applies to `qml/` and its subdirectories]

## OVERVIEW
QML UI modules rendered from `qrc:/qml/*`, consuming C++ manager context properties provided by `src/main_qml.cpp`.

## WHERE TO LOOK
| Task | Location | Notes |
|---|---|---|
| UI entry + popup loaders | `main.qml` | Loads start/settings/log/camera display flows |
| Main operational screen | `content/Start.qml` | Discovery/start flow, manager signal wiring |
| Stream display behavior | `content/CameraDisplay.qml` | Uses `image://videoframe` updates from StreamManager |
| Settings editing UI | `content/Settings.qml` | Binds to ConfigManager and TX/RX options |
| Log UX | `content/Log.qml` | Renders LogManager output/state |
| Module registry | `content/qmldir`, `UntitledProject/qmldir` | Export names used by QML loader/imports |
| Resource manifest | `../qml.qrc` | Source of truth for bundled QML/assets |

## CONVENTIONS (qml-specific)
- Assume runtime paths are qrc-backed; keep resource references compatible with `qml.qrc`.
- Keep manager usage aligned to registered context property names from C++ startup.
- Prefer declarative bindings/signals; push heavy operational logic into C++ managers when adding complexity.
- Treat `.ui.qml` files as design-tool artifacts unless intentionally migrated to hand-edited QML.

## ANTI-PATTERNS
- Do not rename manager property references in QML without matching C++ context registration updates.
- Do not add resource files without adding them to `qml.qrc`.
- Do not hardcode behaviors that duplicate source-of-truth state already emitted by managers.
- Do not rely on deprecated/unused design scaffold modules as primary runtime path.

## UNIQUE STYLES
- Frameless custom title bar and custom window controls in `main.qml`.
- Popup-heavy interaction model for settings/log/stream display.
- UI flow is tightly coupled to live hardware/network manager state transitions.
