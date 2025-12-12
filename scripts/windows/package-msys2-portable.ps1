[CmdletBinding()]
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\..\builddir'),
    [string]$Destination = (Join-Path $PSScriptRoot '..\..\dist\F1sh-Camera-RX'),
    [string]$MsysRoot = $env:MSYS2_ROOT,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
if (-not $MsysRoot) {
    $MsysRoot = 'C:\\msys64'
}
if (-not (Test-Path $MsysRoot)) {
    throw "MSYS2 root '$MsysRoot' was not found. Set MSYS2_ROOT or pass -MsysRoot."
}

$ucrt64Root = Join-Path $MsysRoot 'ucrt64'
if (-not (Test-Path $ucrt64Root)) {
    throw "UCRT64 toolchain directory '$ucrt64Root' is missing. Install MSYS2 UCRT64 packages."
}

$mesonExe = 'meson'
if (-not $SkipBuild) {
    Write-Host 'Building project with Meson...'
    & $mesonExe 'compile' '-C' $BuildDir
    if ($LASTEXITCODE -ne 0) {
        throw 'Meson build failed.'
    }
}

$binaryPath = Join-Path $BuildDir 'f1sh-camera-rx.exe'
if (-not (Test-Path $binaryPath)) {
    throw "Binary not found at '$binaryPath'. Build before packaging."
}

$ntlddExe = Join-Path $ucrt64Root 'bin\ntldd.exe'
if (-not (Test-Path $ntlddExe)) {
    throw "ntldd.exe not found at '$ntlddExe'. Install 'mingw-w64-ucrt-x86_64-ntldd'."
}

if (Test-Path $Destination) {
    Remove-Item -LiteralPath $Destination -Recurse -Force
}

$paths = [ordered]@{
    Root         = (New-Item -ItemType Directory -Path $Destination -Force).FullName
    Bin          = (New-Item -ItemType Directory -Path (Join-Path $Destination 'bin') -Force).FullName
    Lib          = (New-Item -ItemType Directory -Path (Join-Path $Destination 'lib') -Force).FullName
    Plugin       = (New-Item -ItemType Directory -Path (Join-Path $Destination 'lib\gstreamer-1.0') -Force).FullName
    LibExec      = (New-Item -ItemType Directory -Path (Join-Path $Destination 'libexec\gstreamer-1.0') -Force).FullName
    Share        = (New-Item -ItemType Directory -Path (Join-Path $Destination 'share') -Force).FullName
    Schemas      = (New-Item -ItemType Directory -Path (Join-Path $Destination 'share\glib-2.0\schemas') -Force).FullName
    GioModules   = (New-Item -ItemType Directory -Path (Join-Path $Destination 'lib\gio\modules') -Force).FullName
    PixbufLoaders= (New-Item -ItemType Directory -Path (Join-Path $Destination 'lib\gdk-pixbuf-2.0\2.10.0\loaders') -Force).FullName
    Etc          = (New-Item -ItemType Directory -Path (Join-Path $Destination 'etc') -Force).FullName
    SslCerts     = (New-Item -ItemType Directory -Path (Join-Path $Destination 'etc\ssl\certs') -Force).FullName
}

function Copy-TreeOptional {
    param(
        [string]$Source,
        [string]$Destination,
        [string]$Description
    )
    if (Test-Path $Source) {
        Copy-Item -Path $Source -Destination $Destination -Recurse -Force
    } elseif ($Description) {
        Write-Warning "$Description not found at '$Source'. Install the corresponding MSYS2 package or adjust the packaging script."
    }
}

Write-Host 'Copying application binary...'
Copy-Item -LiteralPath $binaryPath -Destination (Join-Path $paths.Bin 'f1sh-camera-rx.exe')
Copy-Item -LiteralPath (Join-Path $repoRoot 'run-portable.cmd') -Destination (Join-Path $paths.Root 'run-portable.cmd') -Force

$copiedDlls = @{}

function Convert-ToWinPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
    $normalized = $Path.Trim()
    if ($normalized -match '^[A-Za-z]:') {
        return ($normalized -replace '/', '\\')
    }
    if ($normalized.StartsWith('/')) {
        if ($normalized.Length -ge 3 -and $normalized[2] -eq '/') {
            $drive = $normalized[1]
            $rest = $normalized.Substring(3)
            return ("{0}:\{1}" -f $drive.ToUpper(), ($rest -replace '/', '\\'))
        }
        return (Join-Path $MsysRoot ($normalized.TrimStart('/') -replace '/', '\\'))
    }
    return $null
}

function Copy-Dll {
    param([string]$SourcePath)
    $name = Split-Path -Path $SourcePath -Leaf
    if ($copiedDlls.ContainsKey($name)) {
        return
    }
    Copy-Item -LiteralPath $SourcePath -Destination (Join-Path $paths.Bin $name) -Force
    $copiedDlls[$name] = $true
}

function Copy-Dependencies {
    param([string]$Binary)
    $output = & $ntlddExe '-R' $Binary
    foreach ($line in $output) {
        if ($line -match '=>\s*([^\s]+)') {
            $candidate = Convert-ToWinPath $matches[1]
            if ($candidate -and (Test-Path $candidate) -and $candidate.StartsWith($ucrt64Root, 'OrdinalIgnoreCase')) {
                Copy-Dll -SourcePath $candidate
            }
        } elseif ($line -match '^\s*([^\s]+\.(dll|DLL))') {
            $candidate = Convert-ToWinPath $matches[1]
            if ($candidate -and (Test-Path $candidate) -and $candidate.StartsWith($ucrt64Root, 'OrdinalIgnoreCase')) {
                Copy-Dll -SourcePath $candidate
            }
        }
    }
}

Copy-Dependencies -Binary $binaryPath

Write-Host 'Copying full GStreamer plugin directory...'
$pluginSource = Join-Path $ucrt64Root 'lib\gstreamer-1.0'
if (-not (Test-Path $pluginSource)) {
    throw "Plugin directory '$pluginSource' not found. Install GStreamer runtime packages."
}
Copy-Item -Path (Join-Path $pluginSource '*') -Destination $paths.Plugin -Recurse -Force
Get-ChildItem -Path $pluginSource -Filter '*.dll' -File | ForEach-Object {
    Copy-Dependencies -Binary $_.FullName
}

Write-Host 'Copying GTK/GStreamer shared data...'
Copy-Item -Path (Join-Path $ucrt64Root 'share\glib-2.0\schemas\*') -Destination $paths.Schemas -Recurse -Force
Copy-TreeOptional -Source (Join-Path $ucrt64Root 'share\icons\Adwaita') -Destination (Join-Path $paths.Share 'icons') -Description 'Adwaita icon theme'
Copy-TreeOptional -Source (Join-Path $ucrt64Root 'share\themes\Adwaita') -Destination (Join-Path $paths.Share 'themes') -Description 'Adwaita GTK theme'
Copy-Item -Path (Join-Path $ucrt64Root 'share\gstreamer-1.0') -Destination (Join-Path $paths.Share 'gstreamer-1.0') -Recurse -Force
Copy-Item -Path (Join-Path $ucrt64Root 'lib\gtk-3.0') -Destination (Join-Path $paths.Lib 'gtk-3.0') -Recurse -Force
Copy-Item -Path (Join-Path $ucrt64Root 'lib\gio\modules\*') -Destination $paths.GioModules -Recurse -Force
Copy-Item -Path (Join-Path $ucrt64Root 'lib\gdk-pixbuf-2.0\2.10.0\loaders\*') -Destination $paths.PixbufLoaders -Recurse -Force
Copy-TreeOptional -Source (Join-Path $ucrt64Root 'etc\gtk-3.0') -Destination (Join-Path $paths.Etc 'gtk-3.0') -Description 'GTK configuration'
Copy-TreeOptional -Source (Join-Path $ucrt64Root 'etc\fonts') -Destination (Join-Path $paths.Etc 'fonts') -Description 'Fontconfig data'
Copy-TreeOptional -Source (Join-Path $ucrt64Root 'etc\pango') -Destination (Join-Path $paths.Etc 'pango') -Description 'Pango modules'

$caBundle = Join-Path $ucrt64Root 'ssl\certs\ca-bundle.crt'
if (Test-Path $caBundle) {
    Copy-Item -LiteralPath $caBundle -Destination (Join-Path $paths.SslCerts 'ca-bundle.crt') -Force
}

$gioQuery = Join-Path $ucrt64Root 'bin\gio-querymodules.exe'
if (Test-Path $gioQuery) {
    & $gioQuery $paths.GioModules | Out-Null
}

$glibCompile = Join-Path $ucrt64Root 'bin\glib-compile-schemas.exe'
if (Test-Path $glibCompile) {
    & $glibCompile $paths.Schemas | Out-Null
}

$pixbufQuery = Join-Path $ucrt64Root 'bin\gdk-pixbuf-query-loaders.exe'
if (Test-Path $pixbufQuery) {
    $env:GDK_PIXBUF_MODULEDIR = $paths.PixbufLoaders
    $loaderCache = & $pixbufQuery
    $loaderCache | Set-Content (Join-Path (Split-Path $paths.PixbufLoaders -Parent) 'loaders.cache') -Encoding ASCII
    Remove-Item Env:\GDK_PIXBUF_MODULEDIR -ErrorAction SilentlyContinue
}

Copy-Item -LiteralPath (Join-Path $ucrt64Root 'libexec\gstreamer-1.0\gst-plugin-scanner.exe') -Destination (Join-Path $paths.LibExec 'gst-plugin-scanner.exe') -Force

Write-Host "Portable package created at $($paths.Root)"
