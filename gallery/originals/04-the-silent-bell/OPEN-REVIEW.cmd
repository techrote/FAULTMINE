@echo off
setlocal DisableDelayedExpansion
set "FM_ART=%~dp0"
for %%I in ("%FM_ART%..\..\..") do set "FM_ROOT=%%~fI"

rem Source path in the authored .fmproj is relative to this gallery root.
rem Do not change source, project, genome or the archived native PNG.
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%FM_ART%review-preflight.ps1"
if errorlevel 1 (
  echo.
  echo [STOP] Original 04 review integrity check failed. No application launched.
  pause
  exit /b 1
)

start "" "%FM_ART%silent-bell-canonical.png"
set "FM_EXE="
if exist "%FM_ROOT%\FAULTMINE.exe" set "FM_EXE=%FM_ROOT%\FAULTMINE.exe"
if not defined FM_EXE if exist "%FM_ROOT%\build\Release\FAULTMINE.exe" set "FM_EXE=%FM_ROOT%\build\Release\FAULTMINE.exe"
if not defined FM_EXE if exist "%FM_ROOT%\build\Debug\FAULTMINE.exe" set "FM_EXE=%FM_ROOT%\build\Debug\FAULTMINE.exe"
if defined FM_EXE (
  start "" /D "%FM_ROOT%" "%FM_EXE%"
) else (
  echo [INFO] No FAULTMINE.exe found in this checkout.
  echo [INFO] The verified native PNG has opened. Use an extracted portable build for GUI review.
)
echo.
echo ART-ORIG-004: The Silent Bell. Draft, not admitted.
echo In FAULTMINE press Ctrl+Shift+O, then open:
echo   gallery\originals\04-the-silent-bell\the-silent-bell.fmproj
echo Compare the GUI canvas with the archived native PNG now open.
echo Then test Ctrl+E still export, overwrite/cancellation and source portability.
echo No physical Win32 GUI acceptance is implied by the preflight or native CLI replay.
echo.
pause
