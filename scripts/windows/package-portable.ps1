param(
    [string]$BuildDir = "..\..\builddir",
    [string]$OutDir = "..\..\dist\F1sh-Camera-RX"
)

$ErrorActionPreference = 'Stop'

function New-Dir($p){ if(-not (Test-Path $p)){ New-Item -ItemType Directory -Force -Path $p | Out-Null } }

# Normalize paths
$BuildDir = Resolve-Path $BuildDir
$OutDir = Resolve-Path -Path $OutDir -ErrorAction SilentlyContinue
if(-not $OutDir){ $OutDir = (Resolve-Path "..\..") + "\dist\F1sh-Camera-RX" }
New-Dir $OutDir

Write-Host "Output: $OutDir"

# Copy main exe
$exe = Join-Path $BuildDir "f1sh-camera-rx.exe"
if(-not (Test-Path $exe)){
  throw "Executable not found at $exe"
}
Copy-Item $exe $OutDir -Force

# Create plugin and aux dirs
$gstPluginsDir1 = Join-Path $OutDir "gstreamer-1.0"
$gstLibexecDir = Join-Path $OutDir "libexec\gstreamer-1.0"
New-Dir $gstPluginsDir1
New-Dir $gstLibexecDir

# Helper: copy a dll if exists
function Copy-Dll($name, $srcDirs){
  foreach($d in $srcDirs){
    $p = Join-Path $d $name
    if(Test-Path $p){ Copy-Item $p $OutDir -Force; return $true }
  }
  return $false
}

# Try to detect pkg roots (adjust these as per your setup)
$roots = @()
$envRoots = @($env:GSTREAMER_1_0_ROOT_MSVC_X86_64, $env:GSTREAMER_1_0_ROOT_MSVC_X86, $env:GSTREAMER_1_0_ROOT_X86_64, $env:GSTREAMER_1_0_ROOT_X86)
foreach($r in $envRoots){ if($r){ $roots += $r } }
# Common MSYS/installed locations
$roots += @("C:\\gstreamer\\1.0\\msvc_x86_64", "C:\\gstreamer\\1.0\\x86_64")

# Copy plugins and scanner from first valid root
$gstRoot = $null
foreach($r in $roots){ if(Test-Path $r){ $gstRoot = $r; break } }
if($gstRoot){
  $pluginsSrc = Join-Path $gstRoot "lib\\gstreamer-1.0"
  if(Test-Path $pluginsSrc){ Copy-Item "$pluginsSrc\\*.dll" $gstPluginsDir1 -Force -ErrorAction SilentlyContinue }
  $scanner1 = Join-Path $gstRoot "libexec\\gstreamer-1.0\\gst-plugin-scanner.exe"
  if(Test-Path $scanner1){ Copy-Item $scanner1 (Join-Path $OutDir "gst-plugin-scanner.exe") -Force }
}

# Try Homebrew-on-Windows style or vcpkg if present
# User can extend below by adding more roots or passing explicit paths

# Locate DLLs from the built exe using dumpbin (if available) to gather dependent DLLs
$deps = @()
try {
  $dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
  $txt = & $dumpbin /DEPENDENTS $exe | Out-String
  $deps = ($txt -split "`n") | Where-Object { $_ -match "\.dll" } | ForEach-Object { ($_ -replace ".*?([A-Za-z0-9._-]+\.dll).*", '$1').Trim() } | Select-Object -Unique
} catch {
  Write-Warning "dumpbin.exe not found; skipping dependent DLL collection"
}

# Candidate search paths for DLLs
$searchDirs = @()
if($gstRoot){
  $searchDirs += @( (Join-Path $gstRoot 'bin'), (Join-Path $gstRoot 'lib') )
}
$searchDirs += @(Split-Path $exe)

foreach($dll in $deps){
  if($dll -match 'vcruntime|msvcp|ucrtbase'){ continue }
  $copied = Copy-Dll $dll $searchDirs
  if(-not $copied){ Write-Host "Missing: $dll" }
}

Write-Host "Portable package created at: $OutDir"
