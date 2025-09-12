@echo off
setlocal
REM Launch helper for portable distribution placed next to this script.
set EXE=%~dp0f1sh-camera-rx.exe
if not exist "%EXE%" (
  echo f1sh-camera-rx.exe not found next to run-portable.cmd
  exit /b 1
)
REM Prefer local dir for DLL search
set PATH=%~dp0;%PATH%
"%EXE%" %*
