@echo off
setlocal DisableDelayedExpansion
set "ART=%~dp0"
for %%I in ("%ART%..\..\..") do set "ROOT=%%~fI"

rem Require a GUI and renderer from the same build; never qualify with one and open the other.
set "BIN="
if exist "%ROOT%\FAULTMINE.exe" if exist "%ROOT%\FAULTMINE-render.exe" set "BIN=%ROOT%"
if not defined BIN if exist "%ROOT%\build\Release\FAULTMINE.exe" if exist "%ROOT%\build\Release\FAULTMINE-render.exe" set "BIN=%ROOT%\build\Release"
if not defined BIN if exist "%ROOT%\build\Debug\FAULTMINE.exe" if exist "%ROOT%\build\Debug\FAULTMINE-render.exe" set "BIN=%ROOT%\build\Debug"
if not defined BIN (
  echo [STOP] No matching GUI/renderer pair. Restore a complete native Windows build.
  pause
  exit /b 1
)
rem Replay must use the exact binary directory chosen for the GUI; this also rejects races.
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ART%review-preflight.ps1" -NativeBinDirectory "%BIN%"
if errorlevel 1 (
  echo.
  echo [STOP] Original 05 review integrity or native replay failed.
  pause
  exit /b 1
)
echo [INFO] Opening original source and authentic Windows CPU canonical pixels.
start "" "%ART%source.png"
start "" "%ART%unclaimed-gesture-canonical.png"
start "" /D "%ROOT%" "%BIN%\FAULTMINE.exe"
echo.
echo ART-ORIG-005: The Unclaimed Gesture. DRAFT, not admitted.
echo In FAULTMINE press Ctrl+Shift+O; load:
echo   gallery\originals\05-the-unclaimed-gesture\the-unclaimed-gesture.fmproj
echo Confirm source identity, compare frame 0 with the archived PNG.
echo Test Ctrl+E export, overwrite and cancellation.
echo Native CLI proof is NOT physical GUI or owner artistic acceptance.
echo.
pause
