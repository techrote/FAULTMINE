@echo off
setlocal DisableDelayedExpansion
set "ART=%~dp0"
for %%I in ("%ART%..\..\..") do set "ROOT=%%~fI"

rem Verify immutable authored evidence before showing any artwork or launching a GUI.
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ART%review-preflight.ps1"
if errorlevel 1 (
  echo.
  echo [STOP] Original 05 review integrity or native replay failed.
  pause
  exit /b 1
)
rem Do not present this as a runnable native review if the GUI executable is absent.
set "APP="
if exist "%ROOT%\FAULTMINE.exe" set "APP=%ROOT%\FAULTMINE.exe"
if not defined APP if exist "%ROOT%\build\Release\FAULTMINE.exe" set "APP=%ROOT%\build\Release\FAULTMINE.exe"
if not defined APP if exist "%ROOT%\build\Debug\FAULTMINE.exe" set "APP=%ROOT%\build\Debug\FAULTMINE.exe"
if not defined APP (
  echo [STOP] Native FAULTMINE.exe is missing. Restore the complete Windows package or build the GUI first.
  pause
  exit /b 1
)
echo [INFO] Opening original source and authentic Windows CPU canonical pixels.
start "" "%ART%source.png"
start "" "%ART%unclaimed-gesture-canonical.png"
start "" /D "%ROOT%" "%APP%"
echo.
echo ART-ORIG-005: The Unclaimed Gesture. DRAFT, not admitted.
echo In FAULTMINE press Ctrl+Shift+O; load:
echo   gallery\originals\05-the-unclaimed-gesture\the-unclaimed-gesture.fmproj
echo Confirm source identity, compare frame 0 with the archived PNG.
echo Test Ctrl+E export, overwrite and cancellation.
echo Native CLI proof is NOT physical GUI or owner artistic acceptance.
echo.
pause
