param(
    [string]$BuildDir = 'build',
    [string]$Configuration = 'Release',
    [string]$OutputDir = 'artifacts'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'
$cmakeText = Get-Content -Raw $cmakePath
$match = [regex]::Match($cmakeText, 'project\(FAULTMINE VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $match.Success) {
    throw 'Unable to resolve FAULTMINE semantic version from CMakeLists.txt.'
}
$version = $match.Groups[1].Value
$archiveBase = "FAULTMINE-$version-win64"
$absoluteBuild = Join-Path $repoRoot $BuildDir
$absoluteOutput = Join-Path $repoRoot $OutputDir
$stageRoot = Join-Path $absoluteOutput 'stage'
$packageRoot = Join-Path $stageRoot $archiveBase
$archivePath = Join-Path $absoluteOutput "$archiveBase.zip"

Remove-Item $stageRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $archivePath -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

& cmake --install $absoluteBuild --config $Configuration --prefix $packageRoot
if ($LASTEXITCODE -ne 0) {
    throw "CMake install failed with exit code $LASTEXITCODE."
}

$launcher = @'
@echo off
setlocal
cd /d "%~dp0"
start "" "FAULTMINE.exe"
'@
Set-Content -Path (Join-Path $packageRoot 'Run FAULTMINE.cmd') -Value $launcher -Encoding ascii -NoNewline

$releaseNote = @"
FAULTMINE $version — Windows x64 portable

Run FAULTMINE.exe directly or use Run FAULTMINE.cmd.
The package uses the statically linked MSVC runtime (/MT) and Windows system Win32/D3D11/WIC components.
No package manager, web runtime, Python, Node.js, installer service, or network account is required.
See docs\RAG_RELEASE_V1.md for release verification, deterministic contracts, resource limits and support diagnostics.
"@
Set-Content -Path (Join-Path $packageRoot 'PORTABLE.txt') -Value $releaseNote -Encoding utf8

Compress-Archive -Path $packageRoot -DestinationPath $archivePath -CompressionLevel Optimal
if (-not (Test-Path $archivePath)) {
    throw 'Portable archive was not created.'
}

Write-Output $archivePath
