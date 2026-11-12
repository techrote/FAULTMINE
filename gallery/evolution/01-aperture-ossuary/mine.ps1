# Runs only the accepted native FAULTMINE-batch.exe; does not emulate its results.
[CmdletBinding()]
param(
    [string]$BinaryPath = '',
    [string]$OutDir = '',
    [ValidateRange(1,32)][int]$Threads = 4,
    [switch]$Resume
)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..\..')).Path
if (-not $BinaryPath) { $BinaryPath = Join-Path $repo 'build\Release\FAULTMINE-batch.exe' }
if (-not $OutDir) { $OutDir = Join-Path $env:TEMP 'FAULTMINE-Aperture-Ossuary-01' }
$source = Join-Path $here 'source.bmp'
$genome = Join-Path $here 'parent-genome.json'
foreach ($file in @($source, $genome, $BinaryPath)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required file not found: $file" }
}
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne '46dc8809a764257cfb61fbec1f85663e64211080a4611606f85fc340ea44ab09') {
    throw 'Original source BMP failed byte-identity check.'
}
if ((Get-FileHash -LiteralPath $genome -Algorithm SHA256).Hash.ToLowerInvariant() -ne '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d') {
    throw 'Pinned accepted parent-genome bytes have changed.'
}
if ((Test-Path -LiteralPath $OutDir) -and (-not $Resume)) {
    throw "Output exists; use a new -OutDir or pass -Resume to validate/reuse native candidate caches: $OutDir"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$argsNative = @('--source', $source, '--genome', $genome, '--out', $OutDir,
    '--seed', '4d13b09ec72a8851', '--begin', '0', '--count', '384', '--threads', "$Threads",
    '--radius', 'high', '--frame', '0', '--render', 'canonical',
    '--near-threshold', '8192', '--select', '12', '--selected-stills')
if ($Resume) { $argsNative += '--resume' }
Write-Host "Executing native canonical mine; source=$source, output=$OutDir"
& $BinaryPath @argsNative
$exit = $LASTEXITCODE
if ($exit -ne 0) { throw "Native batch exit $exit. If interrupted, inspect the strict partial manifest and rerun with -Resume." }
$m = Get-Content -LiteralPath (Join-Path $OutDir 'batch.fmbatch.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not $m.complete -or $m.cancelled -or $m.failures.Count -ne 0 -or $m.candidates.Count -ne 384) {
    throw 'Native manifest is not a completed, failure-free 384-candidate mine.'
}
if ($m.source_identity -ne '0023ceb14294cf69b7db062a1afc255ebaa8cc6e2a0119f4bb81d91be053125a' -or
    $m.parent_genome_identity -ne '01217a9e77ea4cd446b7f6142ab6ea71db9bedbcca48425a02d60ea15849524d' -or
    $m.render_mode -ne 'canonical' -or $m.mutation_seed -ne '4d13b09ec72a8851') {
    throw 'Native manifest source/parent/mode/seed identity mismatch.'
}
$selected = @($m.candidates | Where-Object { $_.selected })
if ($selected.Count -lt 1 -or $selected.Count -gt 12) { throw "Expected 1..12 diverse selections; got $($selected.Count)" }
foreach ($c in $selected) {
    $stem = 'candidate_{0:D20}.png' -f [long]$c.descendant_index
    if (-not (Test-Path (Join-Path $OutDir "selected\$stem"))) { throw "Missing canonical selected still: $stem" }
}
Write-Host "Complete: 384 canonical candidates; $($selected.Count) novelty-selected specimens."
Write-Host "Atlas: $(Join-Path $OutDir 'selected-contact-sheet.png')"
Write-Host "GUI lineage: $(Join-Path $OutDir 'batch.fmproj')"
Write-Host 'For independent decoded-PNG identity checks, run: python qualify-results.py <output-dir> --all'
$selected | Select-Object descendant_index,genome_identity,pixel_identity,novelty_distance |
    Format-Table -AutoSize | Out-Host
