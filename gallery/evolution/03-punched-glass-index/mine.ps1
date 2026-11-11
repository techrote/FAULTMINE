param(
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = (Resolve-Path (Join-Path $here '../../..')).Path
$source = Join-Path $here 'source.bmp'
$genome = Join-Path $root 'examples/fm003-starter-genome.json'
$batch = Join-Path $root 'build/Release/FAULTMINE-batch.exe'
$release = Join-Path $root 'build/Release/FAULTMINE-render.exe'
$debug = Join-Path $root 'build/Debug/FAULTMINE-render.exe'
$out = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $out) { throw "No-overwrite: output already exists: $out" }
foreach ($file in @($source,$genome,$batch,$release,$debug)) {
    if (-not (Test-Path -LiteralPath $file)) { throw "Missing source or native binary: $file" }
}
node (Join-Path $here 'make-source.mjs') --verify
if ($LASTEXITCODE -ne 0) { throw 'Source generator equality failed' }
$sourceSha = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceSha -cne '07b480b94e5f14ed3ac25dd381a47a4ea4e772bf55dd8dd0eeae7ad6e6006881') {
    throw "Authored source hash changed: $sourceSha"
}
$parentBlob = (& git -C $root hash-object -- $genome).Trim()
if ($LASTEXITCODE -ne 0 -or $parentBlob -cne '40e2d690078c62d6c80c40a550165672a2a1f997') {
    throw "Accepted immutable parent changed: $parentBlob"
}
New-Item -ItemType Directory -Force -Path $out | Out-Null
$address = @(
    '--source', $source, '--genome', $genome, '--out', $out,
    '--seed', 'c9a1704efd3268b5', '--begin', '0', '--count', '192',
    '--radius', 'high', '--frame', '0', '--render', 'canonical',
    '--near-threshold', '8192', '--select', '12', '--selected-stills'
)
& $batch @address '--threads' '4'
if ($LASTEXITCODE -ne 0) { throw "Native four-worker mine failed: $LASTEXITCODE" }
$manifestPath = Join-Path $out 'batch.fmbatch.json'
if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Missing native manifest' }
$before = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.manifest_version -ne 1 -or $manifest.descriptor_version -ne 1 -or
    $manifest.mutation_policy_version -ne 1 -or $manifest.radius -cne 'high' -or
    $manifest.mutation_seed -cne 'c9a1704efd3268b5' -or
    $manifest.frame_index -ne 0 -or $manifest.render_mode -cne 'canonical' -or
    $manifest.index_begin -ne 0 -or $manifest.index_end_exclusive -ne 192 -or
    $manifest.near_duplicate_threshold -ne 8192 -or
    $manifest.requested_selection_count -ne 12 -or
    -not $manifest.complete -or $manifest.cancelled -or
    @($manifest.failures).Count -ne 0 -or @($manifest.candidates).Count -ne 192) {
    throw 'Native manifest does not match complete semantic search address'
}
$indices = @($manifest.candidates | ForEach-Object { [long]$_.descendant_index })
if ((@($indices | Select-Object -Unique).Count -ne 192) -or
    $indices[0] -ne 0 -or $indices[191] -ne 191) {
    throw 'Candidate index range is incomplete or disordered'
}
$distinctGenomes = @($manifest.candidates | Select-Object -ExpandProperty genome_identity -Unique).Count
$distinctPixels = @($manifest.candidates | Select-Object -ExpandProperty pixel_identity -Unique).Count
$selected = @($manifest.candidates | Where-Object { $_.selected })
if ($selected.Count -lt 4 -or $distinctPixels -lt 24) {
    throw "Insufficient diversity: $distinctPixels unique pixels, $($selected.Count) selected"
}
if (-not (Test-Path -LiteralPath (Join-Path $out 'selected-contact-sheet.png'))) {
    throw 'Missing native selected contact sheet'
}
foreach ($candidate in $selected) {
    $name = 'candidate_{0:D20}.png' -f [long]$candidate.descendant_index
    $still = Join-Path (Join-Path $out 'selected') $name
    if (-not (Test-Path -LiteralPath $still) -or
        -not (Test-Path -LiteralPath ($still + '.fmmanifest.json'))) {
        throw "Selected FM-012 still or provenance missing at index $($candidate.descendant_index)"
    }
}
& $batch @address '--threads' '1' '--resume'
if ($LASTEXITCODE -ne 0) { throw 'Verified one-worker resume failed' }
$after = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($before -cne $after) { throw 'Semantic manifest changed after alternate-thread verified resume' }
$genomeDir = Join-Path $out 'audit-genomes'
New-Item -ItemType Directory -Force -Path $genomeDir | Out-Null
$inspected = @()
foreach ($candidate in $selected) {
    $index = [long]$candidate.descendant_index
    $name = 'candidate_{0:D20}.png' -f $index
    $still = Join-Path (Join-Path $out 'selected') $name
    $derived = Join-Path $genomeDir ('genome-{0:D20}.json' -f $index)
    [IO.File]::WriteAllText($derived, [string]$candidate.canonical_genome, [Text.UTF8Encoding]::new($false))
    & $release --verify --frame 0 $source $derived $still
    if ($LASTEXITCODE -ne 0) { throw "Release canonical replay mismatch at index $index" }
    if ($inspected.Count -lt 3) {
        & $debug --verify --frame 0 $source $derived $still
        if ($LASTEXITCODE -ne 0) { throw "Debug canonical replay mismatch at index $index" }
    }
    $inspected += [ordered]@{
        index = $index
        candidate_identity = [string]$candidate.candidate_identity
        genome_identity = [string]$candidate.genome_identity
        pixel_identity = [string]$candidate.pixel_identity
        novelty_distance = [long]$candidate.novelty_distance
    }
}
$record = [ordered]@{
    audit_version = 1
    status = 'native_Windows_complete_not_human_curated'
    source_sha256 = $sourceSha
    source_identity = $manifest.source_identity
    parent_genome_identity = $manifest.parent_genome_identity
    mutation_seed = $manifest.mutation_seed
    frame = 0
    render_mode = 'canonical'
    radius = 'high'
    near_threshold_inclusive = 8192
    requested = 192
    completed = @($manifest.candidates).Count
    failures = 0
    unique_genomes = $distinctGenomes
    unique_pixels = $distinctPixels
    selected = $inspected
    native_manifest_sha256 = $after
    resume_threads = @(4,1)
    release_selected_still_replays = $selected.Count
    debug_selected_still_replays = [Math]::Min(3,$selected.Count)
    actual_artistic_selection_pending = $true
}
[IO.File]::WriteAllText((Join-Path $out 'native-audit.json'),
    (($record | ConvertTo-Json -Depth 8) + [char]10), [Text.UTF8Encoding]::new($false))
Write-Host "PASS native canonical mine: $(@($manifest.candidates).Count) candidates, $distinctPixels unique pixels, $($selected.Count) novelty picks, verified resume and Debug/Release replays."
