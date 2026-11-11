# Actual Windows-native CPU CLI frames; not an FM-012 GUI/export receipt.
[CmdletBinding()]
param([ValidateSet('Release','Debug')][string]$Configuration='Release',
      [string]$OutputDirectory='', [string]$RendererPath='')
$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$root=(Resolve-Path (Join-Path $here '..\..\..')).Path
$source=Join-Path $here 'source.bmp'
$genome=Join-Path $here 'genome.json'
if (-not $RendererPath) { $RendererPath=Join-Path $root ("build\"+$Configuration+"\FAULTMINE-render.exe") }
if (-not (Test-Path -LiteralPath $RendererPath -PathType Leaf)) { throw "Missing native renderer: $RendererPath" }
$sourceSha='82180916e814d2738eb8c41e1bf3c745022b219cca788c5467819e83e380fba3'
$sourceId='60352c93cb2322e3d40d2fd77b9793e68cd0cd0314b4e75996113411bd718fe9'
$genomeId='c76d773802faa8e8de5291497dc06e73c277150b65a5779f17dbebaa7d61b189'
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $sourceSha -or
    (Get-FileHash -LiteralPath $genome -Algorithm SHA256).Hash.ToLowerInvariant() -cne $genomeId) {
    throw 'Source/genome mismatch: refusing to render'
}
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $root ('artifacts\rain-glass-'+$Configuration) }
if (Test-Path -LiteralPath $OutputDirectory) { throw "Existing evidence path: $OutputDirectory" }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$audit=[ordered]@{
 audit_version=1;kind='authored-native-cli-not-fm012';complete=$false
 configuration=$Configuration;source_identity=$sourceId;genome_identity=$genomeId
 root_seed='4c6f8a21d3e507b9';frame_begin=0;frame_end_exclusive=24;rate='8/1'
 frames=@();out_of_order_replay=@()
 pending_gates=@('Win32 source relink','FM-012 sequence completion/cancel','Independent visual selection')
}
function Identity([string[]]$lines,[string]$name) {
 $found=@($lines | Where-Object { $_ -cmatch ('^'+[regex]::Escape($name)+'=[0-9a-f]{64}$') })
 if ($found.Count -ne 1) { throw "Missing/ambiguous native identity: $name" }
 return ([string]$found[0]).Substring($name.Length+1)
}
try {
 $records=@()
 for ($frame=0;$frame -lt 24;$frame++) {
   $file=('rain_{0:D6}.png' -f $frame);$png=Join-Path $OutputDirectory $file
   $lines=@(& $RendererPath --frame "$frame" $source $genome $png)
   if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $png -PathType Leaf) -or
       $lines -cnotcontains "frame=$frame") { throw "Native frame $frame failed" }
   if ((Identity $lines 'source_identity') -cne $sourceId) { throw "WIC source identity differs at $frame" }
   $item=[ordered]@{frame=$frame;file=$file;output_identity=(Identity $lines 'output_identity')
     encoded_png_sha256=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash.ToLowerInvariant()}
   $records+=,$item;$audit.frames=$records
 }
 foreach ($frame in @(23,0,12,5,18,9)) {
   $item=$records[$frame];$png=Join-Path $OutputDirectory $item.file
   $lines=@(& $RendererPath --verify --frame "$frame" $source $genome $png)
   if ($LASTEXITCODE -ne 0 -or $lines -cnotcontains 'verification=passed' -or
       $lines -cnotcontains "frame=$frame" -or
       (Identity $lines 'source_identity') -cne $sourceId -or
       (Identity $lines 'output_identity') -cne $item.output_identity) { throw "Replay $frame failed" }
   if ((Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash.ToLowerInvariant() -cne $item.encoded_png_sha256) { throw "Replay wrote frame $frame" }
   $audit.out_of_order_replay+=,$frame
 }
 $audit.complete=$true
} catch { $audit.failure=$_.Exception.Message; throw }
finally {
 $name=if($audit.complete){'native-cli-replay-audit.json'}else{'native-cli-replay-PARTIAL.json'}
 [IO.File]::WriteAllText((Join-Path $OutputDirectory $name),($audit|ConvertTo-Json -Depth 8)+"\n",[Text.UTF8Encoding]::new($false))
}
Write-Host 'PASS: 24 native canonical PNGs and six nonchronological read-only replays'
