param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$preset = if ($Configuration -eq 'Release') { 'windows-release' } else { 'windows-debug' }
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    & ctest --preset $preset
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
