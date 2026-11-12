@echo off
setlocal DisableDelayedExpansion
set "FM_ART=%~dp0"
for %%I in ("%FM_ART%..\..\..") do set "FM_ROOT=%%~fI"

rem Qualify and launch only a matching native GUI/renderer pair from one directory.
set "FM_BIN="
if exist "%FM_ROOT%\FAULTMINE.exe" if exist "%FM_ROOT%\FAULTMINE-render.exe" set "FM_BIN=%FM_ROOT%"
if not defined FM_BIN if exist "%FM_ROOT%\build\Release\FAULTMINE.exe" if exist "%FM_ROOT%\build\Release\FAULTMINE-render.exe" set "FM_BIN=%FM_ROOT%\build\Release"
if not defined FM_BIN if exist "%FM_ROOT%\build\Debug\FAULTMINE.exe" if exist "%FM_ROOT%\build\Debug\FAULTMINE-render.exe" set "FM_BIN=%FM_ROOT%\build\Debug"
if not defined FM_BIN (
 echo [STOP] Missing matching Windows GUI/renderer pair. Restore a complete owner package.
 pause
 exit /b 1
)
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%FM_ART%review-preflight.ps1" -NativeBinDirectory "%FM_BIN%"
if errorlevel 1 (
 echo.
 echo [STOP] Original source, canonical pixels, project, or native CPU replay failed validation.
 pause
 exit /b 1
)
start "" "%FM_ART%source.png"
start "" "%FM_ART%canonical-evidence\witness-relay-canonical.png"
start "" /D "%FM_ROOT%" "%FM_BIN%\FAULTMINE.exe"
echo.
echo [DRAFT] WITNESS RELAY / 02. No artistic or physical GUI acceptance implied.
echo In FAULTMINE press Ctrl+Shift+O and open:
echo gallery\originals\02-witness-relay\witness-relay.fmproj
echo Compare the canvas with the archived canonical PNG; test Ctrl+E export and cancellation.
echo Record owner decision: ADMIT / REVISE / REJECT.
pause
exit /b 0
