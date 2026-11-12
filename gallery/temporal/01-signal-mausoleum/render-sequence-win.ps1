# Execute a bounded native Windows canonical CLI replay of ART-TEMP-001.
# Requires the --frame and --verify contract from integration PR #36 (or accepted successor).
# This is NOT an FM-012 sequence exporter or FM-011 temporal-frame-identity oracle.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$OutputDirectory = '',
    [string]$RendererPath = ''
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$here = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $here '..\..\..')).Path
$source = Join-Path $here 'source.bmp'
$genome = Join-Path $here 'genome.json'
$sourceId = '03502053b9d752dd2db92329108c2d1ed1c769e00db8a08f6ce7b9331992dc75'
$sourceFileSha = 'cbb58573b44b78a7a162289ede71eb541689d078b795960d0504888e3a8090de'
$genomeSha = 'ac6651f58f3ee3b40d3852c6fa9a1435e5de2e6341cd7f5f894306d52695462c'
if ([string]::IsNullOrWhiteSpace($RendererPath)) {
    $RendererPath = Join-Path $repoRoot ("build\" + $Configuration + "\FAULTMINE-render.exe")
}
if (-not (Test-Path -LiteralPath $RendererPath -PathType Leaf)) {
    throw "Missing native Windows renderer: $RendererPath. Build it per the root README, with integration PR #36's --frame/--verify contract."
}
if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
    -not (Test-Path -LiteralPath $genome -PathType Leaf)) {
    throw 'Missing authored source.bmp or genome.json.'
}
# Git on Windows may materialize a canonical LF JSON blob with CRLF line endings.
# Hash its LF-canonical content without modifying the authored file; malformed CR
# sequences and non-UTF8 are still rejected by the strict decoder / identity check.
$genomeText = [IO.File]::ReadAllText($genome, [Text.UTF8Encoding]::new($false, $true))
$genomeCanonical = $genomeText.Replace("`r`n", "`n")
if ($genomeCanonical.Contains("`r")) {
    throw 'Genome contains unsupported bare CR bytes. No render attempted.'
}
$genomeBytes = [Text.Encoding]::UTF8.GetBytes($genomeCanonical)
$genomeCanonicalSha = [BitConverter]::ToString(
    [Security.Cryptography.SHA256]::Create().ComputeHash($genomeBytes)
).Replace('-', '').ToLowerInvariant()
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $sourceFileSha -or
    $genomeCanonicalSha -cne $genomeSha) {
    throw 'Immutable artwork input identity mismatch after LF checkout normalization. No render attempted.'
}

function Invoke-CanonicalCli {
    param([string[]]$CliArguments, [string]$Label)
    $lines = @(& $RendererPath @CliArguments 2>&1 | ForEach-Object { [string]$_ })
    $code = $LASTEXITCODE
    $global:LASTEXITCODE = 0
    if ($code -ne 0) {
        throw "$Label failed with native exit $code. Confirm the Windows executable implements --frame and --verify (integration PR #36). Output: $($lines -join ' | ')"
    }
    return $lines
}
function Read-Identity {
    param([string[]]$Lines, [string]$Name, [string]$Label)
    $pattern = '^' + [regex]::Escape($Name) + '=([0-9a-f]{64})$'
    $matched = @($Lines | Where-Object { $_ -cmatch $pattern })
    if ($matched.Count -ne 1) {
        throw "${Label}: expected exactly one $Name=64-hex identity in native output; got $($Lines -join ' | ')"
    }
    return [regex]::Match([string]$matched[0], $pattern).Groups[1].Value
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot 'artifacts\temporal-signal-mausoleum-native'
}
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Refusing to overwrite/reuse existing output directory: $OutputDirectory. Supply a fresh -OutputDirectory."
}

# Proof results are written only after the renderer has actually run on Windows.
New-Item -ItemType Directory -Path $OutputDirectory -Force -ErrorAction Stop | Out-Null
$audit = [ordered]@{
    audit_version = 1
    kind = 'authoring-native-cli-replay-audit'
    is_fm012_export_manifest = $false
    complete = $false
    native_engine = 'FAULTMINE-render.exe'
    configuration = $Configuration
    source_encoded_sha256 = $sourceFileSha
    source_identity = $sourceId
    genome_identity = $genomeSha
    root_seed = 'a1b2c3d4e5f60718'
    frame_begin = 0
    frame_end_exclusive = 32
    semantic_rate_num = 12
    semantic_rate_den = 1
    frames = @()
    verified_out_of_order = @()
    outstanding_gates = @(
        'FM-012 GUI frame-sequence export manifest with per-frame FM-011 temporal identities',
        'Native project-v2 load/relink and actual visual assessment',
        'Independent FM-012 cancellation/partial-manifest check'
    )
}
try {
    $frames = @()
    for ($frame = 0; $frame -lt 32; $frame++) {
        $filename = ('residual-carrier_{0:D6}.png' -f $frame)
        $destination = Join-Path $OutputDirectory $filename
        $lines = @(Invoke-CanonicalCli -CliArguments @('--frame', [string]$frame, $source, $genome, $destination) -Label "frame $frame canonical render")
        if ($lines -cnotcontains "frame=$frame" -or -not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            throw "frame ${frame}: native output did not include explicit frame/addressed PNG"
        }
        $actualSourceId = Read-Identity $lines 'source_identity' "frame $frame"
        if ($actualSourceId -cne $sourceId) {
            throw "frame ${frame}: native WIC-normalized source identity differs from submitted source identity"
        }
        $renderIdentity = Read-Identity $lines 'output_identity' "frame $frame"
        $record = [ordered]@{
            frame = $frame
            filename = $filename
            canonical_decoded_output_identity = $renderIdentity
            encoded_png_sha256_transport_only = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $frames += $record
        $audit.frames = $frames
        Write-Host "Canonical frame $frame/31: $renderIdentity"
    }
    $audit.frames = $frames

    # Replay in deliberately non-monotonic order. --verify compares the decoded
    # complete RGBA8 result, not PNG container bytes, and must never edit it.
    $probes = @()
    foreach ($frame in @(31, 0, 24, 8, 16)) {
        $record = $frames[$frame]
        $path = Join-Path $OutputDirectory $record.filename
        $encodedBefore = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines = @(Invoke-CanonicalCli -CliArguments @('--verify', '--frame', [string]$frame, $source, $genome, $path) -Label "out-of-order decoded RGBA8 verification frame $frame")
        if ($lines -cnotcontains 'verification=passed' -or $lines -cnotcontains "frame=$frame") {
            throw "frame ${frame}: native verifier did not report passing explicit-frame replay"
        }
        if ((Read-Identity $lines 'source_identity' "verify $frame") -cne $sourceId -or
            (Read-Identity $lines 'output_identity' "verify $frame") -cne $record.canonical_decoded_output_identity) {
            throw "frame ${frame}: source or canonical output identity drift on repeat replay"
        }
        $encodedAfter = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($encodedBefore -cne $encodedAfter) {
            throw "frame ${frame}: read-only verification unexpectedly modified the PNG"
        }
        $probes += $frame
    }
    $audit.verified_out_of_order = $probes
    $audit.complete = $true
    $destinationAudit = Join-Path $OutputDirectory 'native-cli-replay-audit.json'
}
catch {
    $destinationAudit = Join-Path $OutputDirectory 'native-cli-replay-PARTIAL.json'
    $audit.complete = $false
    $reason = $_.Exception.Message
    $audit['failure'] = $reason
    throw
}
finally {
    # Preserve partial progress truthfully. This authoring audit is deliberately
    # not an FM-012 manifest and does not invent temporal frame identities.
    [IO.File]::WriteAllText($destinationAudit,
        (($audit | ConvertTo-Json -Depth 8) + [char]10),
        [Text.UTF8Encoding]::new($false))
    Write-Host "Replay audit: $destinationAudit"
}
Write-Host 'PASS: 32 native frame-addressed PNGs and five out-of-order decoded RGBA8 replays.'
Write-Host 'PENDING: native GUI FM-012 complete/cancelled manifests, FM-011 temporal frame identities and visual acceptance.'
