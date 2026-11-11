# Native-authoritative single-still render and read-only exact-pixel replay.
# This is not an FM-012 GUI export manifest and does not assert aesthetic approval.
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string] $Configuration = 'Release',
    [string] $OutputDirectory = '',
    [switch] $VerifyWithOtherConfiguration
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$here = $PSScriptRoot
$root = (Resolve-Path (Join-Path $here '..\..\..')).Path
$exe = Join-Path $root ("build\" + $Configuration + "\FAULTMINE-render.exe")
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Native $Configuration renderer missing: $exe. Build FAULTMINE on Windows with the root README commands."
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $here 'native-render'
}
$source = Join-Path $here 'source.bmp'
$genome = Join-Path $here 'genome.json'
$destination = Join-Path $OutputDirectory 'moth-rom-canonical.png'
$renderLog = Join-Path $OutputDirectory 'render-stdout.txt'
$verifyLog = Join-Path $OutputDirectory 'verify-stdout.txt'
$otherLog = Join-Path $OutputDirectory 'other-config-verify-stdout.txt'
$receipt = Join-Path $OutputDirectory 'native-replay-receipt.json'
foreach ($path in @($destination,$renderLog,$verifyLog,$otherLog,$receipt)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite prior evidence: $path. Choose an empty OutputDirectory."
    }
}
# Guard author inputs before native execution. In particular, do not trust a
# changed source to remain the same just because its filename is unchanged.
$expectedSourceHash = '41a2be8d45d83c4b15e09ce9d1846256cb89f71d48551f592d1e78f772670e48'
$expectedSourceId = '49aca43e0330c82dec3d579f42f6cca3728603e5155cec434bf0ee81303fff4e'
$expectedGenomeId = 'fa4fda19e0701619a57b2c3eb0ee422ebf8f9f6911537ccc9ee912c125bf72e4'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant() -ne $expectedSourceHash) {
    throw 'Author BMP bytes have changed; refusing to render.'
}
# Git for Windows can check out LF JSON as CRLF. Validate the exact canonical
# LF genome bytes after transport normalization, never a modified genome object.
$genomeText = [IO.File]::ReadAllText($genome, [Text.UTF8Encoding]::new($false))
if ($genomeText.Contains("`r") -and $genomeText.Replace("`r`n",'').Contains("`r")) {
    throw 'Unexpected bare CR in genome; refusing to normalize.'
}
$canonicalGenomeBytes = [Text.UTF8Encoding]::new($false).GetBytes($genomeText.Replace("`r`n","`n"))
$hashEngine = [Security.Cryptography.SHA256]::Create()
try {
    $actualGenomeId = ([Convert]::ToHexString($hashEngine.ComputeHash($canonicalGenomeBytes))).ToLowerInvariant()
} finally {
    $hashEngine.Dispose()
}
if ($actualGenomeId -ne $expectedGenomeId) {
    throw "Canonical genome SHA256 mismatch: $actualGenomeId"
}
$otherExe = $null
if ($VerifyWithOtherConfiguration) {
    $otherConfiguration = if ($Configuration -eq 'Release') { 'Debug' } else { 'Release' }
    $otherExe = Join-Path $root ("build\" + $otherConfiguration + "\FAULTMINE-render.exe")
    if (-not (Test-Path -LiteralPath $otherExe)) {
        throw "Required independent $otherConfiguration renderer missing: $otherExe"
    }
}
if (-not (Test-Path -LiteralPath $OutputDirectory)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}
function Invoke-CheckedNative {
    param([string] $Executable, [string[]] $Arguments, [string] $Log)
    $nativeLines = @(& $Executable @Arguments 2>&1)
    $nativeExit = $LASTEXITCODE
    $global:LASTEXITCODE = 0
    $nativeLines | Out-File -LiteralPath $Log -Encoding utf8
    $nativeLines | ForEach-Object { Write-Host $_ }
    if ($nativeExit -ne 0) {
        throw "Native $Executable failed with exit $nativeExit; see $Log"
    }
    return ,@($nativeLines | ForEach-Object { "$_" })
}
function Require-Identity {
    param([string[]] $Lines, [string] $Key)
    $matches = @($Lines | Where-Object { $_ -cmatch "^$Key=[0-9a-f]{64}$" })
    if ($matches.Count -ne 1) {
        throw "Native result omitted or duplicated exact $Key identity."
    }
    return $matches[0].Substring($Key.Length + 1)
}
Push-Location $root
try {
    $renderLines = Invoke-CheckedNative $exe @('--frame','0',$source,$genome,$destination) $renderLog
    if (-not (Test-Path -LiteralPath $destination)) { throw "Renderer produced no $destination" }
    $sourceId = Require-Identity $renderLines 'source_identity'
    $outputId = Require-Identity $renderLines 'output_identity'
    if ($sourceId -ne $expectedSourceId -or $renderLines -cnotcontains 'frame=0') {
        throw "Native source/frame mismatch: source_identity=$sourceId"
    }
    $encodedBefore = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
    $verifyLines = Invoke-CheckedNative $exe @('--verify','--frame','0',$source,$genome,$destination) $verifyLog
    if ($verifyLines -cnotcontains 'verification=passed' -or $verifyLines -cnotcontains 'frame=0' -or
        (Require-Identity $verifyLines 'source_identity') -ne $sourceId -or
        (Require-Identity $verifyLines 'output_identity') -ne $outputId) {
        throw 'Native same-configuration decoded RGBA8 replay did not prove identical image.'
    }
    if ($null -ne $otherExe) {
        $otherLines = Invoke-CheckedNative $otherExe @('--verify','--frame','0',$source,$genome,$destination) $otherLog
        if ($otherLines -cnotcontains 'verification=passed' -or $otherLines -cnotcontains 'frame=0' -or
            (Require-Identity $otherLines 'source_identity') -ne $sourceId -or
            (Require-Identity $otherLines 'output_identity') -ne $outputId) {
            throw 'Cross-configuration decoded RGBA8 replay mismatch.'
        }
    }
    $encodedAfter = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
    if ($encodedAfter -ne $encodedBefore) {
        throw 'Read-only replay modified the reference PNG.'
    }
    # This author audit is not an FM-012 provenance/export manifest. Canonical
    # authority is the native full-resolution CPU image and decoded RGBA8 replay.
    $audit = [ordered]@{
        audit_version = 1
        kind = 'author-native-cli-replay-receipt-not-fm012'
        source_sha256 = $expectedSourceHash
        source_identity = $sourceId
        genome_identity = $actualGenomeId
        root_seed = '0d07c1cada55e7a1'
        semantic_frame = 0
        engine_contract_version = 1
        render_configuration = $Configuration
        cross_configuration = if ($null -eq $otherExe) { $null } else { $otherConfiguration }
        output_file = 'moth-rom-canonical.png'
        output_identity = $outputId
        encoded_png_sha256 = $encodedAfter
        canonical_rgba_replay = 'passed'
    }
    [IO.File]::WriteAllText($receipt, (($audit | ConvertTo-Json -Depth 5) + "`n"), [Text.UTF8Encoding]::new($false))
    Write-Host "PASS: frame-0 CPU canonical PNG, source/genome identity, read-only decoded-RGBA8 replay; output_identity=$outputId"
    Write-Host "Visual quality and native GUI FM-012 export remain independent acceptance gates."
} finally {
    Pop-Location
}
