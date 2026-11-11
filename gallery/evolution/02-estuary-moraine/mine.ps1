# Native authoritative quarry; do not emulate or represent proxy pixels as canonical.
[CmdletBinding()]
param(
    [string]$BinaryPath = '',
    [string]$OutDir = '',
    [ValidateRange(1,16)][int]$Threads = 4,
    [switch]$Resume
)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..\..')).Path
if (-not $BinaryPath) { $BinaryPath = Join-Path $repo 'build\Release\FAULTMINE-batch.exe' }
if (-not $OutDir) { $OutDir = Join-Path $env:TEMP 'FAULTMINE-Estuary-Moraine-02' }
$source = Join-Path $here 'source.bmp'
$parent = Join-Path $here 'parent-genome.json'
foreach ($f in @($source,$parent,$BinaryPath)) {
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { throw "Missing input: $f" }
}
# The copied parent and author-created source must be *exact* byte-for-byte Git blobs.
$shaSource = (& git -C $repo hash-object -- $source).Trim()
$shaParent = (& git -C $repo hash-object -- $parent).Trim()
if ($shaSource -cne '6696a6f2ff112fef59bb79190da983c319302088' -or
    $shaParent -cne '778727acebba7aa97a8686bec13d38bc222bb430') {
    throw 'Pinned input blob identity mismatch; refusing unqualified source/parent'
}
if ((Test-Path -LiteralPath $OutDir) -and -not $Resume) {
    throw "Output exists; choose fresh -OutDir or explicitly pass -Resume: $OutDir"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$arguments = @('--source',$source,'--genome',$parent,'--out',$OutDir,
    '--seed','e2a7f04c6bd59183','--begin','0','--count','256',
    '--threads',"$Threads",'--radius','high','--frame','0',
    '--render','canonical','--near-threshold','8192','--select','12',
    '--selected-stills')
if ($Resume) { $arguments += '--resume' }
& $BinaryPath @arguments
if ($LASTEXITCODE -ne 0) { throw "FAULTMINE-batch exited $LASTEXITCODE; see partial manifest and use -Resume" }
$m = Get-Content -LiteralPath (Join-Path $OutDir 'batch.fmbatch.json') -Raw -Encoding utf8 | ConvertFrom-Json
if (-not $m.complete -or $m.cancelled -or $m.failures.Count -ne 0 -or
    $m.candidates.Count -ne 256 -or $m.mutation_seed -cne 'e2a7f04c6bd59183' -or
    $m.render_mode -cne 'canonical' -or $m.frame_index -ne 0) {
    throw 'Native batch did not produce the strict complete canonical 256-candidate address'
}
$selected = @($m.candidates | Where-Object selected)
if ($selected.Count -lt 1 -or $selected.Count -gt 12) { throw "Selected count unexpected: $($selected.Count)" }
Write-Host "Native canonical mine: complete=256 selected=$($selected.Count) frame=0; output=$OutDir"
