# Author-scoped driver; no CI claims or GUI/FM-012 substitution.
[CmdletBinding()]
param([string]$OutputDirectory = '',[switch]$CrossCheckDebug)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$here = $PSScriptRoot
$root = (Resolve-Path (Join-Path $here '..\..\..')).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = Join-Path $here 'native-output' }
$source = Join-Path $here 'source.png'
$genome = Join-Path $here 'genome.json'
$release = Join-Path $root 'build\Release\FAULTMINE-render.exe'
$debug = Join-Path $root 'build\Debug\FAULTMINE-render.exe'
if (-not (Test-Path -LiteralPath $release)) { throw "Windows Release renderer missing: $release" }
if ($CrossCheckDebug -and -not (Test-Path -LiteralPath $debug)) { throw "Windows Debug renderer missing: $debug" }
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'b20301c86bbaf0bb1f886fce9fe4f70017286bc0f13b734e261d3a982f049435') {
    throw 'Authored source bytes differ; refusing to render.'
}
$genomeText = [IO.File]::ReadAllText($genome,[Text.UTF8Encoding]::new($false))
if ($genomeText.Contains("`r") -and $genomeText.Replace("`r`n",'').Contains("`r")) { throw 'Bare carriage return in canonical genome' }
$hash = [Security.Cryptography.SHA256]::Create()
try {
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes($genomeText.Replace("`r`n","`n"))
    $gHash = (($hash.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) -join '')
} finally { $hash.Dispose() }
if ($gHash -cne '58f440d271d56e81e94de2f395931de1774af8a92d64627dad9514a41722e313') { throw "Genome identity changed: $gHash" }
$output = Join-Path $OutputDirectory 'witness-relay-canonical.png'
$receipt = Join-Path $OutputDirectory 'native-replay-receipt.json'
if ((Test-Path -LiteralPath $OutputDirectory) -and (Get-ChildItem -LiteralPath $OutputDirectory -Force | Select-Object -First 1)) {
    throw 'Evidence output directory must be empty; refusing overwrite.'
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
function Execute([string]$tool,[string[]]$argv) {
    $lines = @(& $tool @argv 2>&1 | ForEach-Object { "$_" })
    $exit = $LASTEXITCODE
    $global:LASTEXITCODE=0
    if ($exit -ne 0) { throw "Native renderer exited $exit : $($lines -join ' | ')" }
    return ,$lines
}
$render=Execute $release @('--frame','0',$source,$genome,$output)
$sourceID=@($render | Where-Object { $_ -cmatch '^source_identity=[0-9a-f]{64}$' })
$outID=@($render | Where-Object { $_ -cmatch '^output_identity=[0-9a-f]{64}$' })
if ($sourceID.Count -ne 1 -or $sourceID[0] -cne 'source_identity=05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69' -or $outID.Count -ne 1 -or $render -cnotcontains 'frame=0') {
    throw "Native output/source identity or frame missing: $($render -join ' | ')"
}
$encodedBefore = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
foreach ($exe in @($release)+$(if ($CrossCheckDebug) { @($debug) } else { @() })) {
    $check=Execute $exe @('--verify','--frame','0',$source,$genome,$output)
    if ($check -cnotcontains 'verification=passed' -or $check -cnotcontains 'frame=0' -or
        $check -cnotcontains $sourceID[0] -or $check -cnotcontains $outID[0]) {
        throw "Native read-only pixel replay failure for $exe : $($check -join ' | ')"
    }
}
$encodedAfter = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
if ($encodedBefore -cne $encodedAfter) { throw 'Read-only replay changed the canonical PNG transport bytes' }
$audit=[ordered]@{audit_version=1;kind='author-cli-replay-not-FM012';source_identity='05ec476c708b7b569ce4c192b1d32d5270d5fb057619c238dc3d46c40a713b69';
    genome_identity='58f440d271d56e81e94de2f395931de1774af8a92d64627dad9514a41722e313';root_seed='a17e08c4d3ad1e02';frame=0;engine_contract_version=1;
    output_identity=$outID[0].Substring(16);encoded_png_sha256=$encodedAfter;
    release_verify='passed';debug_verify=$(if($CrossCheckDebug){'passed'}else{'not_run'});
    output_file='witness-relay-canonical.png'}
[IO.File]::WriteAllText($receipt,($audit | ConvertTo-Json -Depth 4) + "`n",[Text.UTF8Encoding]::new($false))
Write-Host "PASS: actual Windows canonical CPU render and decoded RGBA8 replay $($outID[0])"
Write-Host "PENDING: actual visual inspection, GUI project load, FM-012 and independent artistic acceptance."
