# Windows-native canonical CPU execution. Neither this audit nor its PNGs are FM-012 GUI exports.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$OutputDirectory = '',
    [string]$RendererPath = ''
)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $here '..\..\..')).Path
$source = Join-Path $here 'source.bmp'
$genome = Join-Path $here 'genome.json'
if (-not $RendererPath) {
    $RendererPath = Join-Path $repoRoot ("build\" + $Configuration + "\FAULTMINE-render.exe")
}
if (-not (Test-Path -LiteralPath $RendererPath -PathType Leaf)) {
    throw "Native Windows renderer missing: $RendererPath"
}
$expectedSourceSha = 'd8bc0cef014b6f7d92b6a53b436c3aa4859eb5b44b8fdb6b52259a812264a6b8'
$expectedSourceId = 'ba533af14e6dc02861ab0d8e55239edcc2358f554e07f460aa4fde9cd590619c'
$expectedGenomeId = '0de88b0fdfbd357ce09761200d2b96b90c8aa2977a9887a3dd7011315751cba2'
$rawGenome = [IO.File]::ReadAllText($genome, [Text.UTF8Encoding]::new($false, $true))
$canonicalGenome = $rawGenome.Replace(([string][char]13 + [string][char]10), [string][char]10)
if ($canonicalGenome.Contains([string][char]13)) { throw 'Unsupported bare CR in genome' }
$sha = [Security.Cryptography.SHA256]::Create()
$genomeDigest = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonicalGenome))).Replace('-', '').ToLowerInvariant()
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expectedSourceSha -or
    $genomeDigest -cne $expectedGenomeId) {
    throw 'Immutable source or genome identity mismatch; refusing native render'
}
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'artifacts\wound-temporal-native' }
if (Test-Path -LiteralPath $OutputDirectory) { throw "No-overwrite evidence gate: $OutputDirectory already exists" }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$audit = [ordered]@{
    audit_version = 1
    kind = 'authored-native-cli-replay-not-fm012'
    complete = $false
    native_configuration = $Configuration
    source_identity = $expectedSourceId
    genome_identity = $expectedGenomeId
    root_seed = 'e30bd49c7a612f08'
    frame_begin = 0
    frame_end_exclusive = 30
    semantic_rate = '12/1'
    frames = @()
    out_of_order_replay = @()
    pending_gates = @('Native Win32 project and source relink', 'Genuine FM-012 complete/cancelled manifests', 'Independent visual selection')
}
function Get-Identity([string[]]$Lines,[string]$Name) {
    $pattern = '^' + [regex]::Escape($Name) + '=[0-9a-f]{64}$'
    $found = @($Lines | Where-Object { $_ -cmatch $pattern })
    if ($found.Count -ne 1) { throw "Expected exactly one native identity for $Name" }
    return ([string]$found[0]).Substring($Name.Length + 1)
}
try {
    $records = @()
    for ($frame = 0; $frame -lt 30; $frame++) {
        $filename = ('reversible-wound_{0:D6}.png' -f $frame)
        $path = Join-Path $OutputDirectory $filename
        $lines = @(& $RendererPath --frame "$frame" $source $genome $path)
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $path -PathType Leaf) -or
            $lines -cnotcontains "frame=$frame") {
            throw "Native canonical render failed at frame $frame with exit code $exitCode"
        }
        if ((Get-Identity $lines 'source_identity') -cne $expectedSourceId) { throw "WIC source identity mismatch at frame $frame" }
        $record = [ordered]@{
            frame = $frame
            file = $filename
            output_identity = (Get-Identity $lines 'output_identity')
            encoded_png_sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $records += $record
        $audit.frames = $records
        Write-Host ("Frame {0:D2} = {1}" -f $frame, $record.output_identity)
    }
    foreach ($frame in @(29, 0, 17, 6, 24)) {
        $record = $records[$frame]
        $path = Join-Path $OutputDirectory $record.file
        $lines = @(& $RendererPath --verify --frame "$frame" $source $genome $path)
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0 -or $lines -cnotcontains 'verification=passed' -or
            $lines -cnotcontains "frame=$frame" -or
            (Get-Identity $lines 'source_identity') -cne $expectedSourceId -or
            (Get-Identity $lines 'output_identity') -cne $record.output_identity) {
            throw "Read-only replay failed for frame $frame"
        }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $record.encoded_png_sha256) {
            throw "Read-only verifier modified frame $frame"
        }
        $audit.out_of_order_replay += $frame
    }
    $audit.complete = $true
}
catch {
    $audit['failure'] = $_.Exception.Message
    throw
}
finally {
    $name = if ($audit.complete) { 'native-cli-replay-audit.json' } else { 'native-cli-replay-PARTIAL.json' }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $name), (($audit | ConvertTo-Json -Depth 8) + [string][char]10), [Text.UTF8Encoding]::new($false))
}
Write-Host 'PASS: Windows native 30-frame canonical CPU PNGs + five sampled read-only replays'
Write-Host 'Not covered: GUI sequence export/cancel, semantic FM-011 frame IDs and artistic acceptance'
