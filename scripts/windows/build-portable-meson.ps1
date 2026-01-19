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

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$DestDir = [System.IO.Path]::GetFullPath($DestDir)

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

$nativeFile = Join-Path $repoRoot 'native-file.ini'

function Invoke-MesonSetup {
    Write-Host "==> Configuring Meson build"
    & meson setup $BuildDir $repoRoot --native-file $nativeFile --prefix="$DestDir"
    if ($LASTEXITCODE -ne 0) {
        throw "Meson setup failed"
    }
}

# Configure build if needed or if existing prefix differs
$needsConfig = -not (Test-Path (Join-Path $BuildDir 'build.ninja'))
if (-not $needsConfig) {
    $infoFile = Join-Path $BuildDir 'meson-info/intro-buildoptions.json'
    if (Test-Path $infoFile) {
        $options = Get-Content $infoFile | ConvertFrom-Json
        $prefixOpt = $options | Where-Object { $_.name -eq 'prefix' }
        if ($prefixOpt -and $prefixOpt.value -ne $DestDir) {
            Write-Host "==> Prefix changed, reconfiguring"
            $needsConfig = $true
        }
    }
}

if (-not $SkipConfigure -and $needsConfig) {
    if (Test-Path $BuildDir) {
        Write-Host "==> Removing stale build directory"
        Remove-Item $BuildDir -Recurse -Force
    }
    Invoke-MesonSetup
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