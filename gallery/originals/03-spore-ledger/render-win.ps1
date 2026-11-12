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
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne '229cf7c953ce5650aec86df9a606b8fa338d4b210085c0d6d938ff6b932f5021') {
    throw 'Authored source bytes differ; refusing to render.'
}
$genomeText = [IO.File]::ReadAllText($genome,[Text.UTF8Encoding]::new($false))
if ($genomeText.Contains("`r") -and $genomeText.Replace("`r`n",'').Contains("`r")) { throw 'Bare carriage return in canonical genome' }
$hash = [Security.Cryptography.SHA256]::Create()
try {
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes($genomeText.Replace("`r`n","`n"))
    $gHash = (($hash.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) -join '')
} finally { $hash.Dispose() }
if ($gHash -cne 'bd645afc85a5f8200a03b273ae2e6c1b5f975384cbe5152ebd9d8620eb206fec') { throw "Genome identity changed: $gHash" }
$output = Join-Path $OutputDirectory 'spore-ledger-canonical.png'
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
    return $lines
}
$render=Execute $release @('--frame','0',$source,$genome,$output)
$sourceID=@($render | Where-Object { $_ -cmatch '^source_identity=[0-9a-f]{64}$' })
$outID=@($render | Where-Object { $_ -cmatch '^output_identity=[0-9a-f]{64}$' })
if ($sourceID.Count -ne 1 -or $sourceID[0] -cne 'source_identity=8afaccd3d00d080b330397f9ab78674832e67ec52c7e65464542d5214d181bc5' -or $outID.Count -ne 1 -or $render -cnotcontains 'frame=0') {
    throw "Native output/source identity or frame missing: $($render -join ' | ')"
}
$encodedBefore = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
$verifyBinaries=@($release)
if ($CrossCheckDebug) { $verifyBinaries += $debug }
foreach ($exe in $verifyBinaries) {
    $check=Execute $exe @('--verify','--frame','0',$source,$genome,$output)
    if ($check -cnotcontains 'verification=passed' -or $check -cnotcontains 'frame=0' -or
        $check -cnotcontains $sourceID[0] -or $check -cnotcontains $outID[0]) {
        throw "Native read-only pixel replay failure for $exe : $($check -join ' | ')"
    }
}
$encodedAfter = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
if ($encodedBefore -cne $encodedAfter) { throw 'Read-only replay changed the canonical PNG transport bytes' }
$audit=[ordered]@{audit_version=1;kind='author-cli-replay-not-FM012';source_identity='8afaccd3d00d080b330397f9ab78674832e67ec52c7e65464542d5214d181bc5';
    genome_identity='bd645afc85a5f8200a03b273ae2e6c1b5f975384cbe5152ebd9d8620eb206fec';root_seed='c6e15af039bd2471';frame=0;engine_contract_version=1;
    output_identity=$outID[0].Substring(16);encoded_png_sha256=$encodedAfter;
    release_verify='passed';debug_verify=$(if($CrossCheckDebug){'passed'}else{'not_run'});
    output_file='spore-ledger-canonical.png'}
[IO.File]::WriteAllText($receipt,($audit | ConvertTo-Json -Depth 4) + "`n",[Text.UTF8Encoding]::new($false))
Write-Host "PASS: actual Windows canonical CPU render and decoded RGBA8 replay $($outID[0])"
Write-Host "PENDING: actual visual inspection, GUI project load, FM-012 and independent artistic acceptance."
