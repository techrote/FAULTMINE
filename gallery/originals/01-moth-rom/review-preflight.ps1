# Fail-closed, read-only Windows owner preflight for MOTH ROM / 07.
[CmdletBinding()]
param([string]$Root = '')
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $here '..\..\..')).Path
} else {
    $Root = (Resolve-Path -LiteralPath $Root).Path
}
function Assert-GitBlob {
    param([string]$Name, [string]$Expected, [bool]$NormalizeText = $false)
    $path = Join-Path $here $Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing authored file: $Name" }
    [byte[]]$data = [IO.File]::ReadAllBytes($path)
    if ($NormalizeText) {
        $utf8 = New-Object System.Text.UTF8Encoding($false, $true)
        $value = $utf8.GetString($data)
        $cr = [string][char]13
        $nl = [string][char]10
        if ($value.Contains($cr) -and $value.Replace(($cr + $nl), '').Contains($cr)) {
            throw "Unexpected bare carriage return: $Name"
        }
        $data = $utf8.GetBytes($value.Replace(($cr + $nl), $nl))
    }
    $prefix = [Text.Encoding]::ASCII.GetBytes(('blob ' + $data.Length + [char]0))
    [byte[]]$joined = New-Object byte[] ($prefix.Length + $data.Length)
    [Array]::Copy($prefix, 0, $joined, 0, $prefix.Length)
    [Array]::Copy($data, 0, $joined, $prefix.Length, $data.Length)
    $sha1 = [Security.Cryptography.SHA1]::Create()
    try {
        $digest = (($sha1.ComputeHash($joined) | ForEach-Object { $_.ToString('x2') }) -join '')
    } finally { $sha1.Dispose() }
    if ($digest -cne $Expected) { throw "Authored Git blob mismatch for $Name : $digest" }
}
$fixed = @(
    @{Name='source.bmp'; Blob='077beedfc984cafe1751d98b9a07108cab39f2fc'; Text=$false},
    @{Name='genome.json'; Blob='4f1d709d7d9d991253b93cb3c75a9ce0e0653c63'; Text=$true},
    @{Name='moth-rom.fmproj'; Blob='721a9f5539c2606763e06c25222ae3fc1bd58a9a'; Text=$true},
    @{Name='moth-rom-canonical.png'; Blob='c7163f5c5437db0d1ef94f8c564a4a213fda3af4'; Text=$false},
    @{Name='artwork.json'; Blob='719477e65df98ba9b92ffd5fb05cf7f98227433e'; Text=$true},
    @{Name='native-qualification.json'; Blob='3c651e1ff00e71a956b83c1e7f964acf31efcbe6'; Text=$true}
)
foreach ($file in $fixed) { Assert-GitBlob $file.Name $file.Blob $file.Text }
$source = Join-Path $here 'source.bmp'
$genome = Join-Path $here 'genome.json'
$canonical = Join-Path $here 'moth-rom-canonical.png'
$project = Get-Content -LiteralPath (Join-Path $here 'moth-rom.fmproj') -Raw -Encoding UTF8 | ConvertFrom-Json
$sourceIdentity = '49aca43e0330c82dec3d579f42f6cca3728603e5155cec434bf0ee81303fff4e'
$outputIdentity = '171e395bac997e35d740f400cbbce3e9fdb556056f5be7fd45eef5d158536c7c'
if ($project.project_version -ne 2 -or
    $project.source.path -cne 'gallery/originals/01-moth-rom/source.bmp' -or
    $project.source.identity -cne $sourceIdentity -or
    $project.lineage.active_genome_identity -cne 'fa4fda19e0701619a57b2c3eb0ee422ebf8f9f6911537ccc9ee912c125bf72e4') {
    throw 'Project v2 source binding, active genome or lineage identity mismatch.'
}
$boundPath = Join-Path $Root ($project.source.path.Replace('/', [IO.Path]::DirectorySeparatorChar))
if (-not (Test-Path -LiteralPath $boundPath -PathType Leaf)) {
    throw "Portable project source path does not resolve at its declared root: $boundPath"
}
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne
    '41a2be8d45d83c4b15e09ce9d1846256cb89f71d48551f592d1e78f772670e48') {
    throw 'Source BMP SHA256 mismatch.'
}
if ((Get-FileHash -LiteralPath $canonical -Algorithm SHA256).Hash.ToLowerInvariant() -cne
    '59488382c0ad776ee00dc1419d11ba1ddbf79871b161059037a6f4234af33088') {
    throw 'Canonical PNG SHA256 mismatch.'
}
$binaryRoots = @($Root, (Join-Path $Root 'build\Release'), (Join-Path $Root 'build\Debug'))
$bin = $null
foreach ($candidate in $binaryRoots) {
    if ((Test-Path -LiteralPath (Join-Path $candidate 'FAULTMINE.exe') -PathType Leaf) -and
        (Test-Path -LiteralPath (Join-Path $candidate 'FAULTMINE-render.exe') -PathType Leaf)) {
        $bin = $candidate
        break
    }
}
if ($null -eq $bin) { throw 'Missing both native FAULTMINE.exe and FAULTMINE-render.exe together (portable root or build/Release or build/Debug).' }
$renderer = Join-Path $bin 'FAULTMINE-render.exe'
Push-Location $Root
try {
    $lines = @(& $renderer '--verify' '--frame' '0' $source $genome $canonical 2>&1 | ForEach-Object { "$_" })
    $exitCode = $LASTEXITCODE
} finally { Pop-Location }
if ($exitCode -ne 0) { throw "Native CPU --verify failed, exit code $exitCode." }
foreach ($value in @('verification=passed','frame=0',"source_identity=$sourceIdentity","output_identity=$outputIdentity")) {
    if (@($lines | Where-Object { $_ -ceq $value }).Count -ne 1) {
        throw "Native replay did not report exactly one '$value'."
    }
}
Write-Host 'PASS: six pinned authored Git blobs (CRLF-safe), exact project binding, source/canonical SHA256 and native CPU frame-0 replay.'
Write-Host 'PENDING: physical GUI import, completed/cancelled FM-012 export and independent aesthetic acceptance.'
