# Windows-only author native CPU/WIC rendering; does not mutate the gallery.
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
if (-not (Test-Path -LiteralPath $release)) { throw "Missing native Windows release renderer: $release" }
if ($CrossCheckDebug -and -not (Test-Path -LiteralPath $debug)) { throw "Missing native Windows Debug renderer: $debug" }
$expectedSource = 'db8754258a9a788d36f4ac466fc538b021725c0cbc7d293c13f8b4c4a005a108'
$expectedGenome = 'f3493de53d45a4013c64488f16fb9b3d1e666f66113b2e996aca2068b4525da4'
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'f7d396be000cd153f29f1b25110162627d0dbf5726be51ea39f35d3d2b005f84') { throw 'Authored source bytes changed' }
if ((Get-FileHash -LiteralPath $genome -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expectedGenome) { throw 'Canonical genome bytes changed' }
if ((Test-Path -LiteralPath $OutputDirectory) -and (Get-ChildItem -LiteralPath $OutputDirectory -Force | Select-Object -First 1)) { throw "Refusing nonempty evidence directory $OutputDirectory" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$out = Join-Path $OutputDirectory 'unclaimed-gesture-canonical.png'
function Invoke-Checked([string]$exe,[string[]]$argv) {
    $lines = @(& $exe @argv 2>&1 | ForEach-Object { "$_" })
    $status = $LASTEXITCODE; $global:LASTEXITCODE = 0
    if ($status -ne 0) { throw "Native renderer $exe exited $status : $($lines -join ' | ')" }
    return $lines
}
$actual = Invoke-Checked $release @('--frame','0',$source,$genome,$out)
$expectedSourceLine = "source_identity=$expectedSource"
if ($actual -cnotcontains $expectedSourceLine -or $actual -cnotcontains 'frame=0') { throw "Source/frame mismatch: $($actual -join ' | ')" }
$lines = @($actual | Where-Object { $_ -cmatch '^output_identity=[0-9a-f]{64}$' })
if ($lines.Count -ne 1) { throw 'No unique canonical identity in native output' }
$pngBefore = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.ToLowerInvariant()
$checks = @($release)
if ($CrossCheckDebug) { $checks += $debug }
foreach ($exe in $checks) {
    $verify = Invoke-Checked $exe @('--verify','--frame','0',$source,$genome,$out)
    if ($verify -cnotcontains 'verification=passed' -or $verify -cnotcontains $expectedSourceLine -or $verify -cnotcontains $lines[0] -or $verify -cnotcontains 'frame=0') {
        throw "Native decoded-pixel replay failed with $exe : $($verify -join ' | ')"
    }
}
$pngAfter = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.ToLowerInvariant()
if ($pngBefore -cne $pngAfter) { throw 'Read-only verification modified output transport' }
$receipt = [ordered]@{ audit_version=1;kind='author-cli-replay-not-FM012';engine_contract_version=1;
    source_identity=$expectedSource;genome_identity=$expectedGenome;root_seed='c18e70d453b29a64';frame=0;
    output_identity=$lines[0].Substring(16);encoded_png_sha256=$pngAfter;release_verify='passed';
    debug_verify=$(if ($CrossCheckDebug) {'passed'} else {'not_run'});output_file='unclaimed-gesture-canonical.png' }
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'native-replay-receipt.json'),(($receipt | ConvertTo-Json -Depth 4)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host "PASS: canonical native render + exact replay, $($lines[0])"
Write-Host 'NEXT: visually inspect actual PNG, independently verify, then decide whether to commit evidence to the artwork branch.'
