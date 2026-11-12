@echo off
setlocal DisableDelayedExpansion
set "FM_ART=%~dp0"
for %%I in ("%FM_ART%..\..\..") do set "FM_ROOT=%%~fI"

rem Source path in the authored .fmproj is relative to this gallery root.
rem Use one matched renderer and GUI; never authenticate pixels with a different build.
set "FM_BIN="
if exist "%FM_ROOT%\FAULTMINE.exe" if exist "%FM_ROOT%\FAULTMINE-render.exe" set "FM_BIN=%FM_ROOT%"
if not defined FM_BIN if exist "%FM_ROOT%\build\Release\FAULTMINE.exe" if exist "%FM_ROOT%\build\Release\FAULTMINE-render.exe" set "FM_BIN=%FM_ROOT%\build\Release"
if not defined FM_BIN if exist "%FM_ROOT%\build\Debug\FAULTMINE.exe" if exist "%FM_ROOT%\build\Debug\FAULTMINE-render.exe" set "FM_BIN=%FM_ROOT%\build\Debug"
if not defined FM_BIN (
  echo [STOP] No matching native Windows GUI/renderer pair. Restore a complete portable or build directory.
  pause
  exit /b 1
)
rem The native preflight verifies the exact same binary directory that the GUI opens.
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%FM_ART%review-preflight.ps1" -NativeBinDirectory "%FM_BIN%"
if errorlevel 1 (
  echo.
  echo [STOP] Original 04 integrity, native pair or frame-zero CPU replay failed.
  pause
  exit /b 1
)
start "" "%FM_ART%source.png"
start "" "%FM_ART%silent-bell-canonical.png"
start "" /D "%FM_ROOT%" "%FM_BIN%\FAULTMINE.exe"
echo.
echo ART-ORIG-004: The Silent Bell. Draft, not admitted.
echo In FAULTMINE press Ctrl+Shift+O, then open:
echo   gallery\originals\04-the-silent-bell\the-silent-bell.fmproj
echo Compare the GUI canvas with the archived native PNG now open.
echo Then test Ctrl+E still export, overwrite/cancellation and source portability.
echo No physical Win32 GUI acceptance is implied by the preflight or native CLI replay.
echo.
pause
