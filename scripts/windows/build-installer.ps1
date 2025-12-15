[CmdletBinding()]
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\..\builddir'),
    [string]$MsysRoot = $env:MSYS2_ROOT,
    [string]$PortableOutput = (Join-Path $PSScriptRoot '..\..\dist\F1sh-Camera-RX'),
    [string]$InstallerOutput = (Join-Path $PSScriptRoot '..\..\dist\installer'),
    [string]$InnoSetup = $(if (Test-Path "${env:ProgramFiles(x86)}") { Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe' } else { Join-Path ${env:ProgramFiles} 'Inno Setup 6\ISCC.exe' }),
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$portableScript = Join-Path $PSScriptRoot 'package-msys2-portable.ps1'
if (-not (Test-Path $portableScript)) {
    throw "Missing helper script: $portableScript"
}

Write-Host '==> Creating portable bundle'
& $portableScript -BuildDir $BuildDir -Destination $PortableOutput -MsysRoot $MsysRoot -SkipBuild:$SkipBuild
if ($LASTEXITCODE -ne 0) {
    throw 'Portable packaging failed.'
}

if (-not (Test-Path $PortableOutput)) {
    throw "Portable directory '$PortableOutput' not found."
}

if (-not (Test-Path $InstallerOutput)) {
    New-Item -ItemType Directory -Path $InstallerOutput -Force | Out-Null
}

if (-not (Test-Path $InnoSetup)) {
    throw "Inno Setup compiler not found at '$InnoSetup'. Install Inno Setup 6 or pass -InnoSetup."
}

$projectInfo = Join-Path $BuildDir 'meson-info\intro-projectinfo.json'
if (-not (Test-Path $projectInfo)) {
    throw "Meson introspection file '$projectInfo' missing. Run 'meson setup' first."
}

$appVersion = (Get-Content $projectInfo | ConvertFrom-Json).version
if (-not $appVersion) {
    $appVersion = '0.0.0'
}

$installerScript = Join-Path $PSScriptRoot 'f1sh-camera-rx-installer.iss'
if (-not (Test-Path $installerScript)) {
    throw "Installer script not found: $installerScript"
}

Write-Host "==> Building installer v$appVersion"
$defines = @(
    "/DAppVersion=$appVersion",
    "/DSourceDir=$PortableOutput",
    "/DOutputDir=$InstallerOutput"
)
& $InnoSetup @defines $installerScript
if ($LASTEXITCODE -ne 0) {
    throw 'Inno Setup compilation failed.'
}

Write-Host "Installer created under $InstallerOutput"
