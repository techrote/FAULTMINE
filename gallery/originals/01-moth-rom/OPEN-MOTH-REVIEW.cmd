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
set "GUI=%ROOT%\FAULTMINE.exe"
if not exist "%GUI%" set "GUI=%ROOT%\build\Release\FAULTMINE.exe"
if not exist "%GUI%" set "GUI=%ROOT%\build\Debug\FAULTMINE.exe"
if not exist "%GUI%" (
  echo FAIL: Native preflight passed but GUI executable disappeared.
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
