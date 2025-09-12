param(
  [string]$BuildDir = "..\..\builddir",
  [string]$OutDir   = "..\..\dist\F1sh-Camera-RX",
  [string]$MsysRoot,               # Optional explicit MSYS2 root (e.g. C:\msys64)
  [string]$DumpbinPath,            # Optional explicit path to dumpbin.exe
  [switch]$VerboseCopy
)

$ErrorActionPreference = 'Stop'
function New-Dir($p){ if(-not (Test-Path $p)){ New-Item -ItemType Directory -Force -Path $p | Out-Null } }
function Copy-All($srcPattern, $dest){ Get-ChildItem -Path $srcPattern -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName -Destination $dest -Force } }

# Resolve paths
$BuildDir = Resolve-Path $BuildDir
$root = Resolve-Path '..\\..'
$OutDir = Resolve-Path -Path $OutDir -ErrorAction SilentlyContinue
if(-not $OutDir){ $OutDir = Join-Path $root 'dist\\F1sh-Camera-RX' }
New-Dir $OutDir

Write-Host "Packaging portable build to $OutDir"

# Copy main exe
$exe = Join-Path $BuildDir 'f1sh-camera-rx.exe'
if(-not (Test-Path $exe)){ throw "Missing exe: $exe" }
Copy-Item $exe $OutDir -Force

# --- MSYS2 root detection (UCRT64 preferred) ---
if($MsysRoot){
  if(-not (Test-Path $MsysRoot)){ throw "Provided -MsysRoot '$MsysRoot' does not exist" }
  $msys = (Resolve-Path $MsysRoot).Path
} else {
  $cands = @()
  if($env:MSYS2_ROOT){ $cands += $env:MSYS2_ROOT }
  $cands += 'C:\\msys64'
  # Try to infer from glib-compile-schemas.exe in PATH
  $glibCmd = Get-Command glib-compile-schemas.exe -ErrorAction SilentlyContinue
  if($glibCmd){
    $binDir = Split-Path $glibCmd.Source -Parent     # .../ucrt64/bin
    $ucrtDir = Split-Path $binDir -Parent            # .../ucrt64
    $maybeRoot = Split-Path $ucrtDir -Parent         # .../msys64
    if(Test-Path $maybeRoot){ $cands = @($maybeRoot) + $cands }
  }
  $msys = $cands | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
  if(-not $msys){ throw "MSYS2 root not found. Install MSYS2 or pass -MsysRoot C:\\msys64." }
}

$ucrtBin = Join-Path $msys 'ucrt64\\bin'
if(-not (Test-Path $ucrtBin)){
  throw "UCRT64 bin not found at '$ucrtBin'. Ensure UCRT64 is installed in MSYS2 or pass a correct -MsysRoot."
}
Write-Host "Using MSYS2 root: $msys"

# DLL list (extended)
$neededPatterns = @(
  'libgtk-3*.dll','libgdk-3*.dll','libgobject-2*.dll','libglib-2*.dll','libgio-2*.dll','libgmodule-2*.dll','libpango-1*.dll','libpangocairo-1*.dll','libatk-1*.dll','libcairo-2*.dll','libgdk_pixbuf-2*.dll',
  'libgst*.dll','libgstaudio*.dll','libgstvideo*.dll','libgstrtp*.dll','libgstrtsp*.dll','libgstbase*.dll','libgstpbutils*.dll','libgstapp*.dll',
  'libcurl*.dll','libjansson*.dll','libmicrohttpd*.dll','libwinpthread-1.dll','zlib1.dll','libpng*.dll','libjpeg*.dll',
  'libidn2-*.dll','libnghttp2-*.dll','libpsl-*.dll','libbrotli*.dll','libzstd*.dll','libssh2-*.dll','libssl-*.dll','libcrypto-*.dll','libiconv-2.dll','libintl-8.dll','libunistring-*.dll',
  'libgnutls-*.dll','libhogweed-*.dll','libnettle-*.dll','libgmp-*.dll','libtasn1-*.dll','libp11-kit-*.dll','libffi-*.dll'
)

foreach($pat in $neededPatterns){
  $matches = Get-ChildItem -Path (Join-Path $ucrtBin $pat) -ErrorAction SilentlyContinue
  foreach($m in $matches){
    if($VerboseCopy){ Write-Host "Copy $($m.Name)" }
    Copy-Item $m.FullName -Destination $OutDir -Force
  }
}

# GStreamer plugins and scanner
$gstPluginsSrc = Join-Path $msys 'ucrt64\\lib\\gstreamer-1.0'
$gstPluginsDest = Join-Path $OutDir 'gstreamer-1.0'
New-Dir $gstPluginsDest
if(Test-Path $gstPluginsSrc){ Copy-All "$gstPluginsSrc\\*.dll" $gstPluginsDest }
$scanner = Join-Path $ucrtBin 'gst-plugin-scanner.exe'
if(Test-Path $scanner){ Copy-Item $scanner (Join-Path $OutDir 'gst-plugin-scanner.exe') -Force }

# Share assets (schemas)
$shareDir = Join-Path $OutDir 'share'
New-Dir $shareDir
$glibSchemasSrc = Join-Path $msys 'ucrt64\\share\\glib-2.0\\schemas'
if(Test-Path $glibSchemasSrc){
  $schemasDest = Join-Path $shareDir 'glib-2.0\\schemas'
  New-Dir $schemasDest
  Copy-All "$glibSchemasSrc\\*.xml" $schemasDest
  $compiler = Join-Path $ucrtBin 'glib-compile-schemas.exe'
  if(Test-Path $compiler){ & $compiler $schemasDest | Out-Null } else { Write-Warning 'glib-compile-schemas.exe not found; GSettings may not work' }
}

# GDK pixbuf loaders cache
$pixbufDir = Join-Path $msys 'ucrt64\\lib\\gdk-pixbuf-2.0\\2.10.0'
if(Test-Path $pixbufDir){
  $destPixbuf = Join-Path $OutDir 'lib\\gdk-pixbuf-2.0\\2.10.0'
  New-Dir $destPixbuf
  New-Dir (Join-Path $destPixbuf 'loaders')
  Copy-All "$pixbufDir\\loaders.cache" $destPixbuf
  Copy-All "$pixbufDir\\loaders\\*.dll" (Join-Path $destPixbuf 'loaders')
}

# Stamp
(Get-Item $exe).VersionInfo | Out-Null
"Portable package created $(Get-Date)" | Out-File (Join-Path $OutDir 'PORTABLE.txt') -Encoding UTF8

# --- Recursive dependency copy using dumpbin/objdump ---
function Find-Dumpbin {
  param([string]$Pref)
  if($Pref -and (Test-Path $Pref)){ return (Resolve-Path $Pref).Path }
  $cmd = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
  if($cmd){ return $cmd.Source }
  $roots = @()
  if($env:VCToolsInstallDir){ $roots += $env:VCToolsInstallDir }
  if($env:VSINSTALLDIR){ $roots += $env:VSINSTALLDIR }
  foreach($r in $roots){
    $cand = Get-ChildItem -Path (Join-Path $r '**/dumpbin.exe') -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if($cand){ return $cand.FullName }
  }
  $vswhere = 'C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe'
  if(Test-Path $vswhere){
    $db = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "VC\\Tools\\MSVC\\**\\bin\\Hostx64\\x64\\dumpbin.exe" 2>$null | Select-Object -First 1
    if($db -and (Test-Path $db)){ return $db }
  }
  return $null
}

function Get-Dependents-Dumpbin([string]$tool,[string]$file){
  $raw = & $tool /DEPENDENTS "$file" 2>$null | Out-String
  $list = @()
  foreach($line in ($raw -split "`n")){
    if($line -match "([A-Za-z0-9._-]+\\.dll)"){
      $dll = $Matches[1].Trim()
      if($dll -notmatch '^(KERNEL32|USER32|GDI32|ADVAPI32|WS2_32|COMDLG32|COMCTL32|SHELL32|OLE32|OLEAUT32|SHLWAPI|IPHLPAPI|CRYPT32|DNSAPI|MSVCRT|UCRTBASE)\\.dll$'){
        $list += $dll
      }
    }
  }
  return ($list | Select-Object -Unique)
}

function Get-Dependents-Objdump([string]$tool,[string]$file){
  $raw = & $tool -p "$file" 2>$null | Out-String
  $list = @()
  foreach($line in ($raw -split "`n")){
    if($line -match "DLL Name:\\s*([A-Za-z0-9._-]+\\.dll)"){
      $dll = $Matches[1].Trim()
      if($dll -notmatch '^(KERNEL32|USER32|GDI32|ADVAPI32|WS2_32|COMDLG32|COMCTL32|SHELL32|OLE32|OLEAUT32|SHLWAPI|IPHLPAPI|CRYPT32|DNSAPI|MSVCRT|UCRTBASE)\\.dll$'){
        $list += $dll
      }
    }
  }
  return ($list | Select-Object -Unique)
}

$depTool = Find-Dumpbin -Pref $DumpbinPath
$depMode = $null
if($depTool){
  $depMode = 'dumpbin'
} elseif(Test-Path (Join-Path $ucrtBin 'objdump.exe')){
  $depTool = (Join-Path $ucrtBin 'objdump.exe')
  $depMode = 'objdump'
}

if($depTool){
  if($VerboseCopy){ Write-Host "Dependency tool: $depTool ($depMode)" }
  $searchBins = @($ucrtBin)
  $changed = $true
  $passes = 0
  while($changed -and $passes -lt 5){
    $passes++
    $changed = $false
    $targets = @((Join-Path $OutDir 'f1sh-camera-rx.exe')) + (Get-ChildItem -Path $OutDir -Filter *.dll -File -Recurse | ForEach-Object { $_.FullName })
    foreach($t in $targets){
      $deps = if($depMode -eq 'dumpbin'){ Get-Dependents-Dumpbin $depTool $t } else { Get-Dependents-Objdump $depTool $t }
      foreach($d in $deps){
        $already = Test-Path (Join-Path $OutDir $d)
        if(-not $already){
          foreach($sb in $searchBins){
            $cand = Join-Path $sb $d
            if(Test-Path $cand){
              if($VerboseCopy){ Write-Host "[dep] $d from $sb" }
              Copy-Item $cand -Destination $OutDir -Force
              $changed = $true
              break
            }
          }
        }
      }
    }
  }
} else {
  Write-Host 'No dumpbin/objdump available; skipped recursive dependency copy'
}

Write-Host 'Done.'
