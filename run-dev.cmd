@echo off
setlocal

REM Set Qt and MinGW paths for development
set "QT_PATH=C:\Qt\6.10.1\mingw_64\bin"
set "MINGW_PATH=C:\Qt\Tools\mingw1310_64\bin"
set "PATH=%QT_PATH%;%MINGW_PATH%;%PATH%"

REM Run the app from builddir
cd /d "%~dp0builddir"
f1sh-camera-rx.exe %*
