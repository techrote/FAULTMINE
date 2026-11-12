# Native Windows author acceptance: deliberately no source or prior-evidence rewrites.
[CmdletBinding()]
param([string]$OutputDirectory = '', [switch]$CrossCheckDebug)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..\..')).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = Join-Path $here 'native-output' }
$source = Join-Path $here 'source.png'
$genome = Join-Path $here 'genome.json'
$release = Join-Path $repo 'build\Release\FAULTMINE-render.exe'
$debug = Join-Path $repo 'build\Debug\FAULTMINE-render.exe'
if (-not (Test-Path -LiteralPath $release)) { throw "Windows Release renderer missing: $release" }
if ($CrossCheckDebug -and -not (Test-Path -LiteralPath $debug)) { throw "Windows Debug renderer missing: $debug" }
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'be6f9c19e0b24544e1fc8883f55a041e06fd309c52bea5771c0bbff8b730da04') { throw 'Source encoding does not match the authored original.' }
if ((Get-FileHash -LiteralPath $genome -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'dc0340692b48259ed77fbf751ed692360c0345945bbb3b50f975ec0c3d8bde61') { throw 'Genome encoding does not match the authored original.' }
if ((Test-Path -LiteralPath $OutputDirectory) -and (Get-ChildItem -LiteralPath $OutputDirectory -Force | Select-Object -First 1)) { throw 'Output directory is not empty; refusing overwrite.' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$out = Join-Path $OutputDirectory 'silent-bell-canonical.png'
function Invoke-Checked([string]$exe,[string[]]$argv) {
    $lines = @(& $exe @argv 2>&1 | ForEach-Object { "$_" })
    $code = $LASTEXITCODE; $global:LASTEXITCODE=0
    if ($code -ne 0) { throw "Native CLI $exe exited $code : $($lines -join ' | ')" }
    return $lines
}
$r = Invoke-Checked $release @('--frame','0',$source,$genome,$out)
$sourceExpected = 'source_identity=275bc5c264376ac81442ca3d5b399cff01b33449c21906be2dc140fe8085e747'
if ($r -cnotcontains $sourceExpected -or $r -cnotcontains 'frame=0') { throw "Wrong source/frame: $($r -join ' | ')" }
$identities = @($r | Where-Object { $_ -cmatch '^output_identity=[0-9a-f]{64}$' })
if ($identities.Count -ne 1) { throw "Missing or duplicate authoritative native output ID: $($r -join ' | ')" }
$before = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.ToLowerInvariant()
$executables = @($release)
if ($CrossCheckDebug) { $executables += $debug }
foreach ($exe in $executables) {
    $v = Invoke-Checked $exe @('--verify','--frame','0',$source,$genome,$out)
    if ($v -cnotcontains 'verification=passed' -or $v -cnotcontains $sourceExpected -or $v -cnotcontains $identities[0] -or $v -cnotcontains 'frame=0') { throw "Decoded-pixel native replay mismatch: $exe" }
}
$after = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.ToLowerInvariant()
if ($before -cne $after) { throw 'Read-only replay modified canonical PNG transport.' }
$receipt = [ordered]@{audit_version=1;kind='author-cli-replay-not-FM012';engine_contract_version=1;
    source_identity=$sourceExpected.Substring(16);genome_identity='dc0340692b48259ed77fbf751ed692360c0345945bbb3b50f975ec0c3d8bde61';
    root_seed='9f6b31ca0d42e857';frame=0;output_identity=$identities[0].Substring(16);
    encoded_png_sha256=$after;release_verify='passed';debug_verify=$(if ($CrossCheckDebug) {'passed'} else {'not_run'});
    output_file='silent-bell-canonical.png'}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'native-replay-receipt.json'),(($receipt | ConvertTo-Json -Depth 4)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host "PASS: canonical native RGBA replay, $($identities[0])"
Write-Host 'PENDING: actual image inspection, GUI project reopen and independent admission.'
