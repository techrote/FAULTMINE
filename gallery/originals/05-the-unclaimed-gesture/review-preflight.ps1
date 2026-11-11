# ART-ORIG-005 portable read-only Windows preflight. No authored file is rewritten.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$art = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = [IO.Path]::GetFullPath((Join-Path $art '..\..\..'))
$expected = [ordered]@{
  'source.png' = 'f7d396be000cd153f29f1b25110162627d0dbf5726be51ea39f35d3d2b005f84'
  'genome.json' = 'f3493de53d45a4013c64488f16fb9b3d1e666f66113b2e996aca2068b4525da4'
  'the-unclaimed-gesture.fmproj' = '14880df2f490aed709f998d1d52e004eb344fd9c4245ee9f97f53ed20160bfbe'
  'unclaimed-gesture-canonical.png' = 'fb3272681ad20147035e9209ed767d1b2d6e4ea26b3a44c7407cf1b27feb07d4'
  'native-replay-receipt.json' = '09371ca0192c7f13115c88a4fea22682e5521d9905412ffba2e0d48826b17631'
  'artwork.json' = '7ffc8c0660c8fea3b1b1953346b99e1a6fdcaec7f851130016662bfdec9b71ec'
}
try {
  foreach ($name in $expected.Keys) {
    $path = Join-Path $art $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing required author asset: $name" }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -cne $expected[$name]) { throw "SHA-256 mismatch for $name; expected $($expected[$name]); found $actual" }
  }
  $project = Get-Content -LiteralPath (Join-Path $art 'the-unclaimed-gesture.fmproj') -Raw | ConvertFrom-Json
  $sourcePath = 'gallery/originals/05-the-unclaimed-gesture/source.png'
  $sourceIdentity = 'db8754258a9a788d36f4ac466fc538b021725c0cbc7d293c13f8b4c4a005a108'
  if ($project.project_version -ne 2 -or $project.source.path -cne $sourcePath -or
      $project.source.identity -cne $sourceIdentity) {
    throw 'The project v2 source binding differs from the original.'
  }
  if (-not (Test-Path -LiteralPath (Join-Path $root 'gallery\originals\05-the-unclaimed-gesture\source.png') -PathType Leaf)) {
    throw 'The original source does not resolve from the application root.'
  }
  $manifest = Get-Content -LiteralPath (Join-Path $art 'artwork.json') -Raw | ConvertFrom-Json
  $outputIdentity = '981bf2f2bb64f69cb8da54a59a343af8b3b9c0c51828bccddff924973b672979'
  if ($manifest.canonical_output_identity -cne $outputIdentity -or
      $manifest.canonical_output_file -cne 'unclaimed-gesture-canonical.png') {
    throw 'The canonical output record differs from the native Windows proof.'
  }
  Write-Host '[PASS] Six original SHA-256 identities and portable project source path.'
  $renderers = @(
    (Join-Path $root 'FAULTMINE-render.exe'),
    (Join-Path $root 'build\Release\FAULTMINE-render.exe'),
    (Join-Path $root 'build\Debug\FAULTMINE-render.exe')
  )
  $renderer = $renderers | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
  if ($null -ne $renderer) {
    $lines = @(& $renderer --verify --frame 0 (Join-Path $art 'source.png') (Join-Path $art 'genome.json') (Join-Path $art 'unclaimed-gesture-canonical.png') 2>&1 | ForEach-Object { "$_" })
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -or $lines -cnotcontains 'verification=passed' -or
        $lines -cnotcontains "source_identity=$sourceIdentity" -or
        $lines -cnotcontains "output_identity=$outputIdentity" -or
        $lines -cnotcontains 'frame=0') {
      throw "Read-only native canonical replay failed ($exitCode): $($lines -join '; ')"
    }
    Write-Host '[PASS] Windows CPU canonical frame-0 read-only replay.'
  } else {
    Write-Host '[NOTE] No native renderer found; hash checks passed but native replay was not performed.'
  }
  exit 0
} catch {
  Write-Error "[STOP] Original 05 preflight: $($_.Exception.Message)"
  exit 1
}
