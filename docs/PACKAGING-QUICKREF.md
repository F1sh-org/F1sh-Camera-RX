# Windows Packaging Quick Reference

## New Meson-Based Approach (Recommended)

### Build Portable Bundle
```powershell
pwsh -File scripts\windows\build-portable-meson.ps1
```
**Output:** `dist/F1sh-Camera-RX/` (60-80 MB)

### Build Installer
```powershell
pwsh -File scripts\windows\build-installer-meson.ps1
```
**Output:** `dist/installer/F1sh-Camera-RX-Setup-<version>.exe`

### Common Options
```powershell
# Skip build (already compiled)
-SkipBuild

# Skip Meson configuration
-SkipConfigure

# Custom paths
-BuildDir "path\to\builddir"
-DestDir "path\to\output"
-MsysRoot "D:\msys64"
```

## Key Benefits
- ✅ **70% smaller bundles** (12 plugins vs 100+)
- ✅ **Faster builds** (less file copying)
- ✅ **Better maintainability** (Meson integration)
- ✅ **Automatic dependency resolution**

## Files Created
| Script | Purpose |
|--------|---------|
| [build-portable-meson.ps1](../scripts/windows/build-portable-meson.ps1) | Meson-based portable bundle builder |
| [build-installer-meson.ps1](../scripts/windows/build-installer-meson.ps1) | Meson-based installer builder |
| [meson-post-install.py](../scripts/windows/meson-post-install.py) | Python dependency bundling script |

## Documentation
- **Full docs:** [MESON-PACKAGING.md](MESON-PACKAGING.md)
- **Legacy docs:** [PACKAGING.md](PACKAGING.md)

---

## Legacy Approach (Deprecated)

```powershell
# Old scripts (still work but create larger bundles)
pwsh -File scripts\windows\package-msys2-portable.ps1
pwsh -File scripts\windows\build-installer.ps1
```

**Output:** `dist/F1sh-Camera-RX/` (200+ MB)