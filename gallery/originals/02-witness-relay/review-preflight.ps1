# Read-only, fail-closed Windows owner review preflight; never creates or changes artwork.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$art = $PSScriptRoot
$root = (Resolve-Path -LiteralPath (Join-Path $art '..\..\..')).Path
$source = Join-Path $art 'source.png'
$genome = Join-Path $art 'genome.json'
$project = Join-Path $art 'witness-relay.fmproj'
$canonical = Join-Path $art 'canonical-evidence\witness-relay-canonical.png'
$receipt = Join-Path $art 'canonical-evidence\native-replay-receipt.json'
function Require-Sha([string]$path,[string]$expected) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing required native review evidence: $path. Use the full owner ZIP, not an author-only source checkout." }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -cne $expected) { throw "SHA-256 mismatch: $path (got $actual)" }
}
try {
    Require-Sha $source 'b20301c86bbaf0bb1f886fce9fe4f70017286bc0f13b734e261d3a982f049435'
    Require-Sha $genome '58f440d271d56e81e94de2f395931de1774af8a92d64627dad9514a41722e313'
    Require-Sha $project '2a38f533e45e4169e9cf8bf2618c77fa683b58f8e58e2645639f32bf38aa66ee'
    Require-Sha (Join-Path $art 'artwork.json') '415faafe7d8a04cec05580c712851a61af84bc2c7b1a517b356e5a6eaec18907'
    Require-Sha $canonical 'e132120a1515a8a70f5e504a6993788ff500ce8aa60f85d531c5eee0d9a1c67a'
    Require-Sha $receipt '78a1a41e9ba73ecace073a7f8b2fbfb2b4e0a603b5598a17e0374e7d07ccc841'
    $p = Get-Content -LiteralPath $project -Raw | ConvertFrom-Json
    if ($p.project_version -ne 2 -or $p.source.path -cne 'gallery/originals/02-witness-relay/source.png' -or $p.source.identity -cne '05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69') { throw 'Project source binding is not portable or differs from the author.' }
    $r = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if ($r.frame -ne 0 -or $r.root_seed -cne 'a17e08c4d3ad1e02' -or
        $r.source_identity -cne '05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69' -or
        $r.genome_identity -cne '58f440d271d56e81e94de2f395931de1774af8a92d64627dad9514a41722e313' -or
        $r.output_identity -cne '34009717b2be5aec243ce405d608c5bda001933b8f898cedf4e153caf2ba728c' -or
        $r.encoded_png_sha256 -cne 'e132120a1515a8a70f5e504a6993788ff500ce8aa60f85d531c5eee0d9a1c67a' -or
        $r.release_verify -cne 'passed' -or $r.debug_verify -cne 'passed') { throw 'Native receipt does not match the frozen Windows evidence.' }
    $renderer = @((Join-Path $root 'FAULTMINE-render.exe'), (Join-Path $root 'build\Release\FAULTMINE-render.exe'), (Join-Path $root 'build\Debug\FAULTMINE-render.exe')) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    $gui = @((Join-Path $root 'FAULTMINE.exe'), (Join-Path $root 'build\Release\FAULTMINE.exe'), (Join-Path $root 'build\Debug\FAULTMINE.exe')) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $renderer) { throw 'Missing actual native FAULTMINE-render.exe. No hash-only pass is allowed.' }
    if (-not $gui) { throw 'Missing actual native FAULTMINE.exe. No image-only pass is allowed.' }
    $nativeLines = @(& $renderer --verify --frame 0 $source $genome $canonical 2>&1 | ForEach-Object { "$_" })
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -or
        $nativeLines -cnotcontains 'verification=passed' -or $nativeLines -cnotcontains 'frame=0' -or
        $nativeLines -cnotcontains 'source_identity=05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69' -or
        $nativeLines -cnotcontains 'output_identity=34009717b2be5aec243ce405d608c5bda001933b8f898cedf4e153caf2ba728c') {
        throw "Native CPU decoded replay failed (exit $exitCode): $($nativeLines -join ' | ')"
    }
    Write-Host '[PASS] Original source/project, archived native PNG and actual Windows CPU decoded-pixel replay.'
    exit 0
} catch {
    Write-Host "[STOP] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
