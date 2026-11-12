# Read-only owner preflight. Source/genome/project/evidence bytes are never rewritten.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$art = $PSScriptRoot
$root = (Resolve-Path -LiteralPath (Join-Path $art '..\..\..')).Path
$source = Join-Path $art 'source.png'
$genome = Join-Path $art 'genome.json'
$project = Join-Path $art 'spore-ledger.fmproj'
$canonical = Join-Path $art 'canonical-evidence\spore-ledger-canonical.png'
$receipt = Join-Path $art 'canonical-evidence\native-replay-receipt.json'
function Require-Sha([string]$path,[string]$expected) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing required artwork: $path. Extract the complete review ZIP before launching." }
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -cne $expected) { throw "SHA-256 mismatch for $path (actual $actual)" }
}
try {
    Require-Sha $source '229cf7c953ce5650aec86df9a606b8fa338d4b210085c0d6d938ff6b932f5021'
    Require-Sha $genome 'bd645afc85a5f8200a03b273ae2e6c1b5f975384cbe5152ebd9d8620eb206fec'
    Require-Sha $project '065615737bc1b59d25bfff03a0c55b99977957c7fe4232cd060a23efbe0717c5'
    Require-Sha (Join-Path $art 'artwork.json') 'cf9cee83bc76fc05ea7613c451a119037713d06b7d958258f767cec9b5bfd7ba'
    Require-Sha $canonical 'a6fec5a377f00c2b0517f1f1c9411c94817a6abc11500fafee14563798f08959'
    Require-Sha $receipt 'e7b4335168d4a908d9e20bbc1abfeaad5470dbc42cc27f4522b3aeb0fe967d7c'
    $p = Get-Content -LiteralPath $project -Raw | ConvertFrom-Json
    $projectSource = 'gallery/originals/03-spore-ledger/source.png'
    if ($p.project_version -ne 2 -or $p.source.path -cne $projectSource -or
        $p.source.identity -cne '8afaccd3d00d080b330397f9ab78674832e67ec52c7e65464542d5214d181bc5') { throw 'Project source identity or portable root-relative source path differs.' }
    $resolvedSource = Join-Path $root ($p.source.path.Replace('/', '\'))
    if (-not (Test-Path -LiteralPath $resolvedSource -PathType Leaf)) { throw "Project source does not resolve from package root: $resolvedSource" }
    if ((Get-FileHash -LiteralPath $resolvedSource -Algorithm SHA256).Hash.ToLowerInvariant() -cne '229cf7c953ce5650aec86df9a606b8fa338d4b210085c0d6d938ff6b932f5021') { throw 'Project source resolved to different pixels.' }
    $r = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if ($r.kind -cne 'author-cli-replay-not-FM012' -or $r.frame -ne 0 -or
        $r.root_seed -cne 'c6e15af039bd2471' -or
        $r.output_identity -cne '8cf910493a00d66eb57970e23d3fc374fa9f581707546038692de2699eb0ed23' -or
        $r.release_verify -cne 'passed' -or $r.debug_verify -cne 'passed') { throw 'Archived Windows replay receipt is inconsistent.' }
    # Never pair a renderer from one build with a GUI from another.
    $bin = $null
    foreach ($candidate in @($root, (Join-Path $root 'build\Release'), (Join-Path $root 'build\Debug'))) {
        if ((Test-Path -LiteralPath (Join-Path $candidate 'FAULTMINE-render.exe') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $candidate 'FAULTMINE.exe') -PathType Leaf)) {
            $bin = $candidate
            break
        }
    }
    if (-not $bin) { throw 'Native FAULTMINE.exe and FAULTMINE-render.exe must coexist in the portable root or the same build configuration.' }
    $renderer = Join-Path $bin 'FAULTMINE-render.exe'
    $gui = Join-Path $bin 'FAULTMINE.exe'
    $originalHash = (Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash.ToLowerInvariant()
    $lines = @(& $renderer --verify --frame 0 $source $genome $canonical 2>&1 | ForEach-Object { "$_" })
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -or $lines -cnotcontains 'verification=passed' -or $lines -cnotcontains 'frame=0' -or
        $lines -cnotcontains 'source_identity=8afaccd3d00d080b330397f9ab78674832e67ec52c7e65464542d5214d181bc5' -or
        $lines -cnotcontains 'output_identity=8cf910493a00d66eb57970e23d3fc374fa9f581707546038692de2699eb0ed23') { throw "Native read-only CPU replay failed (exit $exitCode): $($lines -join ' | ')" }
    if ((Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash.ToLowerInvariant() -cne $originalHash) { throw 'Native verification unexpectedly rewrote the canonical PNG.' }
    Write-Host '[PASS] Immutable assets, portable project source and native CPU canonical frame-zero replay.'
    exit 0
} catch {
    Write-Host "[STOP] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
