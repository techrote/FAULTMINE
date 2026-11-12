@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "ART=%~dp0"
for %%I in ("%ART%..\..\..") do set "ROOT=%%~fI"
echo [MOTH ROM / 07] Read-only native Windows preflight
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%ART%review-preflight.ps1" -Root "%ROOT%"
if errorlevel 1 (
  echo.
  echo FAIL: Native or authored-asset verification did not pass. No artwork or GUI opened.
  pause
  exit /b 1
)
rem Mirror review-preflight.ps1's root / Release / Debug selection exactly.
rem Never open a root-only GUI while the CPU replay used a different build.
set "GUI="
if exist "%ROOT%\FAULTMINE.exe" if exist "%ROOT%\FAULTMINE-render.exe" set "GUI=%ROOT%\FAULTMINE.exe"
if not defined GUI if exist "%ROOT%\build\Release\FAULTMINE.exe" if exist "%ROOT%\build\Release\FAULTMINE-render.exe" set "GUI=%ROOT%\build\Release\FAULTMINE.exe"
if not defined GUI if exist "%ROOT%\build\Debug\FAULTMINE.exe" if exist "%ROOT%\build\Debug\FAULTMINE-render.exe" set "GUI=%ROOT%\build\Debug\FAULTMINE.exe"
if not defined GUI (
  echo FAIL: Matching native GUI/renderer pair vanished after preflight.
  pause
  exit /b 1
)
echo PASS: verified canonical frame zero. Opening the archived artwork and FAULTMINE.
start "" "%ART%moth-rom-canonical.png"
start "" /D "%ROOT%" "%GUI%"
echo.
echo In FAULTMINE press Ctrl+Shift+O and open:
echo   %ART%moth-rom.fmproj
echo Test completed and cancelled Ctrl+E exports.
echo This launcher is not human GUI or aesthetic acceptance.
pause
