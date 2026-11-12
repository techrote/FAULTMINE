@echo off
setlocal DisableDelayedExpansion
set "FM_ART=%~dp0"
for %%I in ("%FM_ART%..\..\..") do set "FM_ROOT=%%~fI"
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%FM_ART%review-preflight.ps1"
if errorlevel 1 (
 echo.
 echo [STOP] Native CPU identity, original source or project could not be verified. No image or application launched.
 pause
 exit /b 1
)
set "FM_GUI="
if exist "%FM_ROOT%\FAULTMINE.exe" set "FM_GUI=%FM_ROOT%\FAULTMINE.exe"
if not defined FM_GUI if exist "%FM_ROOT%\build\Release\FAULTMINE.exe" set "FM_GUI=%FM_ROOT%\build\Release\FAULTMINE.exe"
if not defined FM_GUI if exist "%FM_ROOT%\build\Debug\FAULTMINE.exe" set "FM_GUI=%FM_ROOT%\build\Debug\FAULTMINE.exe"
if not defined FM_GUI (
 echo [STOP] Missing native GUI. Restore complete owner package.
 pause
 exit /b 1
)
start "" "%FM_ART%source.png"
start "" "%FM_ART%canonical-evidence\witness-relay-canonical.png"
start "" /D "%FM_ROOT%" "%FM_GUI%"
echo.
echo [DRAFT] WITNESS RELAY / 02. No artistic or physical GUI acceptance implied.
echo In the application press Ctrl+Shift+O and open:
echo gallery\originals\02-witness-relay\witness-relay.fmproj
echo Compare the canvas with the archived canonical PNG; test Ctrl+E export and cancellation.
echo Record owner decision: ADMIT / REVISE / REJECT.
pause
exit /b 0
