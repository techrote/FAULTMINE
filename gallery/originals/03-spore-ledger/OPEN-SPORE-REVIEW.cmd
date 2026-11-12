@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "ART=%~dp0"
for %%I in ("%ART%..\..\..") do set "ROOT=%%~fI"
if not exist "%ART%review-preflight.ps1" (
  echo [STOP] Missing Spore Ledger native preflight.
  pause
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ART%review-preflight.ps1"
if errorlevel 1 (
  echo [STOP] Native verification failed. Artwork and application have not been opened.
  pause
  exit /b 1
)
rem Choose the same paired-native build as the PowerShell preflight.
set "BIN="
if exist "%ROOT%\FAULTMINE.exe" if exist "%ROOT%\FAULTMINE-render.exe" set "BIN=%ROOT%"
if not defined BIN if exist "%ROOT%\build\Release\FAULTMINE.exe" if exist "%ROOT%\build\Release\FAULTMINE-render.exe" set "BIN=%ROOT%\build\Release"
if not defined BIN if exist "%ROOT%\build\Debug\FAULTMINE.exe" if exist "%ROOT%\build\Debug\FAULTMINE-render.exe" set "BIN=%ROOT%\build\Debug"
if not defined BIN (
  echo [STOP] No matching FAULTMINE.exe and FAULTMINE-render.exe pair in a single native build.
  pause
  exit /b 1
)
echo [PASS] Native frame verified without rewriting the evidence.
echo Opening genuine archived canonical still and FAULTMINE GUI.
echo Once open: Ctrl+Shift+O and choose gallery\originals\03-spore-ledger\spore-ledger.fmproj.
echo Compare to archived still. Test Ctrl+E export, cancellation, and decide ADMIT/REVISE/REJECT.
start "" "%ART%canonical-evidence\spore-ledger-canonical.png"
rem Project source paths are repository-root-relative; build/Release is not a valid working directory.
start "" /D "%ROOT%" "%BIN%\FAULTMINE.exe"
if errorlevel 1 (
  echo [STOP] Windows reported a launch error. Physical GUI acceptance is not established.
  pause
  exit /b 1
)
echo Physical GUI behavior and artistic quality require human assessment.
exit /b 0
