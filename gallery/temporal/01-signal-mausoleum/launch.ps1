# Launch existing native FAULTMINE from the repository root for relative source paths.
$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$exe = Join-Path $repositoryRoot 'build\Release\FAULTMINE.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Release native executable missing: $exe. Configure/build FAULTMINE per the root README."
}
Write-Host "In FAULTMINE: Ctrl+Shift+O => gallery/temporal/01-signal-mausoleum/signal-mausoleum.fmproj"
Push-Location $repositoryRoot
try { & $exe } finally { Pop-Location }
