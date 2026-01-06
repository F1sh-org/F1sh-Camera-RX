[CmdletBinding()]
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\..\builddir'),
    [string]$DestDir = (Join-Path $PSScriptRoot '..\..\dist\F1sh-Camera-RX'),
    [string]$MsysRoot = $env:MSYS2_ROOT,
    [switch]$SkipBuild,
    [switch]$SkipConfigure
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..').Path

if (-not $MsysRoot) {
    $MsysRoot = 'C:\msys64'
}

if (-not (Test-Path $MsysRoot)) {
    throw "MSYS2 root '$MsysRoot' not found. Set MSYS2_ROOT or pass -MsysRoot."
}

$env:MSYS2_ROOT = $MsysRoot

Write-Host "==> Meson-based Windows Packaging"
Write-Host "Build dir:  $BuildDir"
Write-Host "Output dir: $DestDir"
Write-Host "MSYS2 root: $MsysRoot"
Write-Host ""

# Configure build if needed
if (-not $SkipConfigure -and -not (Test-Path (Join-Path $BuildDir 'build.ninja'))) {
    Write-Host "==> Configuring Meson build"
    $nativeFile = Join-Path $repoRoot 'native-file.ini'

    & meson setup $BuildDir --native-file $nativeFile --prefix="$DestDir"
    if ($LASTEXITCODE -ne 0) {
        throw "Meson setup failed"
    }
}

# Build
if (-not $SkipBuild) {
    Write-Host "==> Building project"
    & meson compile -C $BuildDir
    if ($LASTEXITCODE -ne 0) {
        throw "Meson compile failed"
    }
}

# Clean destination
if (Test-Path $DestDir) {
    Write-Host "==> Cleaning old destination"
    Remove-Item -Path $DestDir -Recurse -Force
}

# Install (this triggers the post-install script)
Write-Host "==> Installing and packaging"
& meson install -C $BuildDir --no-rebuild
if ($LASTEXITCODE -ne 0) {
    throw "Meson install failed"
}

Write-Host ""
Write-Host "==> Portable bundle created successfully!"
Write-Host "Location: $DestDir"
Write-Host ""
Write-Host "To test, run: $DestDir\run-portable.cmd"