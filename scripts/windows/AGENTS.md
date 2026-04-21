# AGENTS.md - windows packaging
[Scope: Applies to `scripts/windows/`]

## OVERVIEW
Windows distribution pipeline: Meson install hook + Python post-install bundling + PowerShell wrappers for portable and installer outputs.

## WHERE TO LOOK
| Task | Location | Notes |
|---|---|---|
| Packaging core | `meson-post-install.py` | Copies curated plugins, resolves DLL deps, runs windeployqt |
| Portable bundle command | `build-portable-meson.ps1` | Meson setup/compile/install wrapper |
| Installer command | `build-installer-meson.ps1` | Calls portable build then Inno Setup compiler |
| Installer recipe | `f1sh-camera-rx-installer.iss` | Output layout, metadata, installer behavior |

## CONVENTIONS (windows-specific)
- Packaging is Meson-driven (`meson install`) and expects post-install script execution.
- Use MSYS2 UCRT64 toolchain/runtime (`ntldd`, `windeployqt6`, `glib-compile-schemas`) as dependency source.
- Keep `required_plugins` list in `meson-post-install.py` synchronized with active `src/streammanager.cpp` pipeline elements.
- Preserve `run-portable.cmd` environment contract (`GST_PLUGIN_PATH`, `QML2_IMPORT_PATH`, `SSL_CERT_FILE`, schema paths).

## ANTI-PATTERNS
- Do not copy entire GStreamer plugin tree by default; project policy is selective plugin bundling.
- Do not bypass `meson-post-install.py` when validating distributable output.
- Do not treat deprecated legacy packaging scripts/docs as canonical workflow.
- Do not assume system Qt/GStreamer installs at runtime; bundle integrity is required.

## VALIDATION
```powershell
pwsh -File scripts/windows/build-portable-meson.ps1
pwsh -File scripts/windows/build-installer-meson.ps1
```
