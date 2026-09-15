param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$preset = if ($Configuration -eq 'Release') { 'windows-release' } else { 'windows-debug' }
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    & cmake --build --preset $preset --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
