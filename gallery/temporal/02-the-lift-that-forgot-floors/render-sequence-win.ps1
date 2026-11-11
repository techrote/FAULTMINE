# Real Windows CPU-only canonical CLI frames and read-only sampled replay.
# This produces an authoring replay audit, NOT an FM-012 GUI manifest.
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
    throw "Native renderer missing: $RendererPath"
}
$expectedSourceSha = '9ffbeb836d5437487d45dce6cdb394d7d2c08992edb9deba387cab8025a1986f'
$expectedSourceId = '906aaeb14df9d961ae302c641b053e7db5cf5e13305abeb67ea438733d736d60'
$expectedGenomeSha = '075c3dadcf6b9d4357f8ff6295c066497e7cb97083f91557b52a3ca18dc999f0'
$rawGenome = [IO.File]::ReadAllText($genome, [Text.UTF8Encoding]::new($false, $true))
$canonicalGenome = $rawGenome.Replace("`r`n", "`n")
if ($canonicalGenome.Contains("`r")) { throw 'Malformed genome: bare carriage return' }
$genomeSha = [BitConverter]::ToString(([Security.Cryptography.SHA256]::Create()).ComputeHash([Text.Encoding]::UTF8.GetBytes($canonicalGenome))).Replace('-', '').ToLowerInvariant()
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expectedSourceSha -or
    $genomeSha -cne $expectedGenomeSha) { throw 'Authored input bytes mismatch: aborting without rendering' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'artifacts\lift-temporal-native' }
if (Test-Path -LiteralPath $OutputDirectory) { throw "Refusing to overwrite evidence: $OutputDirectory" }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$audit = [ordered]@{
    audit_version = 1
    kind = 'native-cpu-cli-replay-not-fm012'
    complete = $false
    native_configuration = $Configuration
    source_identity = $expectedSourceId
    genome_identity = $expectedGenomeSha
    root_seed = '67da08c2be71fa30'
    frame_begin = 0
    frame_end_exclusive = 24
    semantic_rate = '10/1'
    frames = @()
    out_of_order_replay = @()
    missing_gates = @('FM-012 GUI complete/cancelled sequence manifests', 'native GUI v2 project load/relink', 'independent visual selection')
}
function Get-Id([string[]]$Lines, [string]$Name) {
    $found = @($Lines | Where-Object { $_ -cmatch ('^' + [regex]::Escape($Name) + '=[0-9a-f]{64}$') })
    if ($found.Count -ne 1) { throw "Expected exactly one ${Name} in native CLI output" }
    return ([string]$found[0]).Substring($Name.Length + 1)
}
try {
    $records = @()
    for ($frame = 0; $frame -lt 24; $frame++) {
        $name = ('mechanical-amnesia_{0:D6}.png' -f $frame)
        $path = Join-Path $OutputDirectory $name
        $lines = @(& $RendererPath --frame "$frame" $source $genome $path)
        $exit = $LASTEXITCODE; $global:LASTEXITCODE = 0
        if ($exit -ne 0 -or -not (Test-Path $path) -or $lines -cnotcontains "frame=$frame") { throw "Render failed at frame $frame, exit=$exit" }
        if ((Get-Id $lines 'source_identity') -cne $expectedSourceId) { throw "Source identity mismatch at frame $frame" }
        $record = [ordered]@{frame = $frame; file = $name; output_identity = (Get-Id $lines 'output_identity'); encoded_png_sha256 = (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        $records += $record
        $audit.frames = $records
        Write-Host "Canonical frame $frame : $($record.output_identity)"
    }
    foreach ($frame in @(23, 0, 18, 6, 12)) {
        $record = $records[$frame]
        $path = Join-Path $OutputDirectory $record.file
        $lines = @(& $RendererPath --verify --frame "$frame" $source $genome $path)
        $exit = $LASTEXITCODE; $global:LASTEXITCODE = 0
        if ($exit -ne 0 -or $lines -cnotcontains 'verification=passed' -or
            $lines -cnotcontains "frame=$frame" -or (Get-Id $lines 'source_identity') -cne $expectedSourceId -or
            (Get-Id $lines 'output_identity') -cne $record.output_identity) { throw "Read-only pixel replay mismatch at frame $frame" }
        if ((Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $record.encoded_png_sha256) {
            throw "Read-only verifier modified frame $frame"
        }
        $audit.out_of_order_replay += $frame
    }
    $audit.complete = $true
} catch {
    $audit['failure'] = $_.Exception.Message
    throw
} finally {
    $name = if ($audit.complete) { 'native-cli-replay-audit.json' } else { 'native-cli-replay-PARTIAL.json' }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $name), (($audit | ConvertTo-Json -Depth 7) + "`n"), [Text.UTF8Encoding]::new($false))
}
Write-Host 'PASS: real 24-frame Windows-native canonical CPU output and five nonchronological read-only decodes'
Write-Host 'Not proven by this CLI helper: FM-012 export, GUI workflow, aesthetic acceptance'
