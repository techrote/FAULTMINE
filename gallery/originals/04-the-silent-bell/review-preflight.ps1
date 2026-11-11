# ART-ORIG-004 read-only Windows preflight. No files are rewritten.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$art = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = [IO.Path]::GetFullPath((Join-Path $art '..\..\..'))
$expected = [ordered]@{
  'source.png' = 'be6f9c19e0b24544e1fc8883f55a041e06fd309c52bea5771c0bbff8b730da04'
  'genome.json' = 'dc0340692b48259ed77fbf751ed692360c0345945bbb3b50f975ec0c3d8bde61'
  'the-silent-bell.fmproj' = '0b801f455b410aa115e22d863903178e5e52450aa38b39bdbb8259f328d1b597'
  'silent-bell-canonical.png' = 'e9b492eddbc4d16ee528c18a236b0930c278aa8909ecdca3d68ef07c4e1d657a'
  'native-replay-receipt.json' = 'b1e1659ea62eaefe77db686856cdd33864f67acc2cbcf421b9646b89a8630a5c'
  'artwork.json' = '61e438b87aab18d48046c04df1fa6af565d4f4a8fdf66dd3b9a7ed72fe347760'
}
try {
  foreach ($name in $expected.Keys) {
    $path = Join-Path $art $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
      throw "Missing original asset: $name"
    }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -cne $expected[$name]) {
      throw "Identity mismatch for $name; expected $($expected[$name]), found $actual"
    }
  }
  $project = Get-Content -LiteralPath (Join-Path $art 'the-silent-bell.fmproj') -Raw | ConvertFrom-Json
  $expectedPath = 'gallery/originals/04-the-silent-bell/source.png'
  if ($project.project_version -ne 2 -or $project.source.path -cne $expectedPath -or
      $project.source.identity -cne '275bc5c264376ac81442ca3d5b399cff01b33449c21906be2dc140fe8085e747') {
    throw 'The authored v2 project source binding changed; refuse silent relinking.'
  }
  $sourceAtProjectPath = Join-Path $root 'gallery\originals\04-the-silent-bell\source.png'
  if (-not (Test-Path -LiteralPath $sourceAtProjectPath -PathType Leaf)) {
    throw "Project-relative source path does not resolve from the application root: $sourceAtProjectPath"
  }
  $manifest = Get-Content -LiteralPath (Join-Path $art 'artwork.json') -Raw | ConvertFrom-Json
  if ($manifest.canonical_output_file -cne 'silent-bell-canonical.png' -or
      $manifest.canonical_output_identity -cne 'd1778bc3d688e5fa5694485954c268cce88c54cc033a6933d3edf2311cf35b8e') {
    throw 'Canonical manifest identity or output path differs from the verified Windows record.'
  }
  Write-Host '[PASS] Original source, genome, project, native PNG, receipt and manifest: 6/6 SHA256 matches.'
  Write-Host '[PASS] Portable v2 project source path resolves from the application root.'

  $renderers = @(
    (Join-Path $root 'FAULTMINE-render.exe'),
    (Join-Path $root 'build\Release\FAULTMINE-render.exe'),
    (Join-Path $root 'build\Debug\FAULTMINE-render.exe')
  )
  $renderer = $renderers | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
  if ($null -ne $renderer) {
    & $renderer --verify (Join-Path $art 'source.png') (Join-Path $art 'genome.json') (Join-Path $art 'silent-bell-canonical.png')
    if ($LASTEXITCODE -ne 0) { throw "Native read-only canonical replay failed with exit code $LASTEXITCODE" }
    Write-Host '[PASS] Read-only native canonical frame-0 replay against archived CPU pixels.'
  } else {
    Write-Host '[NOTE] Native CLI unavailable here: hashes checked; canonical replay not performed.'
  }
  exit 0
} catch {
  Write-Error "[STOP] Silent Bell preflight: $($_.Exception.Message)"
  exit 1
}
