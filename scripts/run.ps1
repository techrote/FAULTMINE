param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $repoRoot "build\$Configuration\FAULTMINE.exe"

if (-not (Test-Path $executable)) {
    throw "FAULTMINE executable not found at '$executable'. Configure and build $Configuration first."
}

& $executable
