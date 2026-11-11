# Executes the real native Windows reference renderer at explicit frame 0.
# Output is deliberately kept uncommitted until independently qualified.
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string] $Configuration = 'Release',
    [string] $OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$root = (Resolve-Path (Join-Path $here '..\..\..')).Path
$exe = Join-Path $root ("build\" + $Configuration + "\FAULTMINE-render.exe")
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Native $Configuration renderer missing: $exe. Build FAULTMINE on Windows with the root README commands."
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $here 'native-render'
}
if (-not (Test-Path -LiteralPath $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
}
$source = Join-Path $here 'source.bmp'
$genome = Join-Path $here 'genome.json'
$destination = Join-Path $OutputDirectory 'moth-rom-canonical.png'
$log = Join-Path $OutputDirectory 'render-stdout.txt'
if (Test-Path -LiteralPath $destination) {
    throw "Existing output is preserved: $destination. Choose an empty OutputDirectory."
}
Push-Location $root
try {
    $lines = & $exe $source $genome $destination 2>&1
    $exit = $LASTEXITCODE
    $lines | Out-File -LiteralPath $log -Encoding utf8
    $lines | ForEach-Object { Write-Host $_ }
    if ($exit -ne 0) { throw "Native FAULTMINE renderer failed with exit $exit; see $log" }
    if (-not (Test-Path -LiteralPath $destination)) { throw "Renderer succeeded without producing $destination" }
    $encoded = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
    Write-Host "Encoded PNG SHA256 (NOT canonical pixel identity): $encoded"
    Write-Host "Obtain the canonical decoded output image identity from the native renderer stdout above."
    Write-Host "Integration must independently WIC-redecode and compare exact pixels (PR #36 --verify when available)."
} finally { Pop-Location }
