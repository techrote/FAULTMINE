# ART-ORIG-004 Windows read-only preflight regression; works without a compiler.
# Copies only the frozen six files plus preflight into a disposable temporary tree.
# No authored project/source, cached output or application file is ever modified.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('faultmine-orig04-review-' + [Guid]::NewGuid().ToString('N'))
$probe = Join-Path $tempRoot 'gallery\originals\04-the-silent-bell'
$originals = @(
  'source.png', 'genome.json', 'the-silent-bell.fmproj',
  'silent-bell-canonical.png', 'native-replay-receipt.json', 'artwork.json'
)
function Invoke-Probe {
  & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $probe 'review-preflight.ps1') -AllowHashOnly *> $null
  return ($LASTEXITCODE -eq 0)
}
try {
  New-Item -ItemType Directory -Force -Path $probe | Out-Null
  foreach ($name in @('review-preflight.ps1') + $originals) {
    Copy-Item -LiteralPath (Join-Path $here $name) -Destination (Join-Path $probe $name)
  }
  # The ordinary owner-facing preflight must NOT pass in an asset-only copy.
  # Only the explicit disposable-test flag may skip the native renderer.
  & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $probe 'review-preflight.ps1') *> $null
  if ($LASTEXITCODE -eq 0) { throw 'Native-required preflight accepted a missing renderer.' }
  Write-Host '[PASS] Normal reviewer preflight rejects missing native renderer.'
  if (-not (Invoke-Probe)) { throw 'Intact reviewer copy rejected.' }
  Write-Host '[PASS] Untouched, self-contained original 04 is accepted.'
  foreach ($name in $originals) {
    $path = Join-Path $probe $name
    $unaltered = [IO.File]::ReadAllBytes($path)
    if ($unaltered.Length -lt 1) { throw "Unexpected empty asset: $name" }
    $changed = [byte[]]$unaltered.Clone()
    $changed[0] = $changed[0] -bxor 1
    [IO.File]::WriteAllBytes($path, $changed)
    $accepted = Invoke-Probe
    [IO.File]::WriteAllBytes($path, $unaltered)
    if ($accepted) { throw "Tampered $name was accepted." }
    if (-not (Invoke-Probe)) { throw "Restored $name was rejected." }
    Write-Host "[PASS] Reject modified $name; pass after exact restoration."
  }
  # A portable project can be intact but referenced source missing at launch.
  Remove-Item -LiteralPath (Join-Path $probe 'source.png') -Force
  if (Invoke-Probe) { throw 'Missing portable source was accepted.' }
  Write-Host '[PASS] Missing project-bound source rejected.'
  Write-Host '[PASS] Native-required rejection, 6 tamper cases, 6 restores and one missing-input case.'
  Write-Host '[NOTE] Test uses the hash-only temporary copy; native CLI replay and physical GUI require independent Windows qualification.'
  exit 0
} finally {
  if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
