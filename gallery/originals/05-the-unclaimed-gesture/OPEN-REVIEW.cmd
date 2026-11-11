@echo off
setlocal
set "ART=%~dp0"
for %%I in ("%ART%..\..\..") do set "REPO=%%~fI"
if not exist "%ART%unclaimed-gesture-canonical.png" (
  echo Missing archived canonical PNG. & exit /b 1
)
echo Opening genuine Windows CPU canonical frame 0.
start "" "%ART%unclaimed-gesture-canonical.png"
set "APP=%REPO%\build\Release\FAULTMINE.exe"
if not exist "%APP%" set "APP=%REPO%\FAULTMINE-1.0.0-win64\FAULTMINE.exe"
if not exist "%APP%" (
  echo No FAULTMINE Release executable found; archived image is still viewable.
  echo Build Release or use the verified owner-review portable ZIP.
  exit /b 0
)
pushd "%REPO%" || exit /b 1
start "" "%APP%"
popd
echo In FAULTMINE use Ctrl+Shift+O to open:
echo   gallery\originals\05-the-unclaimed-gesture\the-unclaimed-gesture.fmproj
echo Physical project loading/export and aesthetic acceptance remain pending.
