param(
  [Parameter(Mandatory=$true)][string]$OutputDirectory,
  [ValidateRange(1,8)][int]$Threads=4
)
$ErrorActionPreference='Stop'
$here=(Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$root=(Resolve-Path (Join-Path $here '../../..')).Path
$source=Join-Path $here 'source.bmp'
$parent=Join-Path $root 'examples/fm003-starter-genome.json'
$batch=Join-Path $root 'build/Release/FAULTMINE-batch.exe'
$debug=Join-Path $root 'build/Debug/FAULTMINE-render.exe'
$release=Join-Path $root 'build/Release/FAULTMINE-render.exe'
$out=[IO.Path]::GetFullPath($OutputDirectory)
foreach($path in @($source,$parent,$batch,$debug,$release)){
  if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing required asset or native executable: $path"}
}
if(-not(Get-Command git -ErrorAction SilentlyContinue)){throw 'Git is required to verify the immutable original source and parent blobs'}
$sourceBlob=(& git -C $root hash-object -- $source).Trim()
if($LASTEXITCODE -ne 0 -or $sourceBlob -cne '7186d6dc03dc60032ca64e891ad4a6d5e50786db'){throw "Source Git blob mismatch: $sourceBlob"}
$parentBlob=(& git -C $root hash-object -- $parent).Trim()
if($LASTEXITCODE -ne 0 -or $parentBlob -cne '40e2d690078c62d6c80c40a550165672a2a1f997'){throw "Accepted immutable parent changed: $parentBlob"}
if(Test-Path -LiteralPath $out){throw "Refusing existing output directory (including partials); preserve old evidence: $out"}
New-Item -Path $out -ItemType Directory -Force | Out-Null
$argsBatch=@('--source',$source,'--genome',$parent,'--out',$out,
  '--seed','8a7c1940e6db2f53','--begin','0','--count','144',
  '--radius','high','--frame','0','--render','canonical',
  '--near-threshold','10000','--select','12','--selected-stills')
& $batch @argsBatch '--threads' "$Threads"
$exit=$LASTEXITCODE;$global:LASTEXITCODE=0
if($exit -ne 0){throw "Native canonical miner failed ($exit). Partial evidence preserved at $out"}
$manifestPath=Join-Path $out 'batch.fmbatch.json'
if(-not(Test-Path -LiteralPath $manifestPath)){throw 'No semantic native batch manifest'}
$m=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$results=@($m.candidates);$failures=@($m.failures)
if($m.manifest_version -ne 1 -or $m.descriptor_version -ne 1 -or
  $m.mutation_policy_version -ne 1 -or $m.mutation_seed -cne '8a7c1940e6db2f53' -or
  $m.radius -cne 'high' -or $m.frame_index -ne 0 -or $m.render_mode -cne 'canonical' -or
  $m.index_begin -ne 0 -or $m.index_end_exclusive -ne 144 -or
  $m.near_duplicate_threshold -ne 10000 -or $m.requested_selection_count -ne 12 -or
  -not $m.complete -or $m.cancelled -or $results.Count -ne 144 -or $failures.Count -ne 0){
  throw 'Native manifest incomplete or differs from the explicit mine address'
}
for($i=0;$i -lt 144;$i++){
  if([long]$results[$i].descendant_index -ne $i){throw "Index order violation at $i"}
}
$uniqueGenomes=@($results | Select-Object -ExpandProperty genome_identity -Unique).Count
$uniquePixels=@($results | Select-Object -ExpandProperty pixel_identity -Unique).Count
$selected=@($results | Where-Object {$_.selected})
$nearRepresentatives=@($results | Where-Object {$_.near_representative_index -eq $_.descendant_index})
if($uniqueGenomes -lt 40 -or $uniquePixels -lt 30 -or $selected.Count -lt 6){
  throw "Insufficient diversity for artistic review: genomes=$uniqueGenomes pixels=$uniquePixels selections=$($selected.Count)"
}
$sheet=Join-Path $out 'selected-contact-sheet.png'
if(-not(Test-Path -LiteralPath $sheet)){throw 'Missing genuine native selection contact sheet'}
$preResume=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
& $batch @argsBatch '--threads' '1' '--resume'
$exit=$LASTEXITCODE;$global:LASTEXITCODE=0
if($exit -ne 0){throw "One-worker verified resume failed ($exit)"}
$postResume=(Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
if($preResume -cne $postResume){throw 'Manifest changed after verified different-thread resume'}
$replays=0
foreach($c in $selected){
  $i=[long]$c.descendant_index
  $name=('candidate_{0:D20}.png' -f $i)
  $png=Join-Path (Join-Path $out 'selected') $name
  $sidecar=$png+'.fmmanifest.json'
  if(-not(Test-Path -LiteralPath $png) -or -not(Test-Path -LiteralPath $sidecar)){
    throw "Missing full-resolution selected still/FM-012 sidecar: $name"
  }
  $genomeText=[string]$c.canonical_genome
  $temporary=Join-Path $out ('audit-genome-{0:D20}.json' -f $i)
  [IO.File]::WriteAllText($temporary,$genomeText,[Text.UTF8Encoding]::new($false))
  $beforePng=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash
  foreach($exe in @($release,$debug)){
    $lines=@(& $exe '--verify' '--frame' '0' $source $temporary $png)
    $exit=$LASTEXITCODE;$global:LASTEXITCODE=0
    if($exit -ne 0 -or $lines -cnotcontains 'verification=passed' -or
      $lines -cnotcontains "output_identity=$($c.pixel_identity)"){
      throw "Native read-only canonical replay failed index=$i exe=$exe :: $($lines -join '; ')"
    }
    $replays++
  }
  if((Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash -cne $beforePng){
    throw "Read-only replay altered PNG at index=$i"
  }
}
$audit=[ordered]@{
  record_version=1; source='04-hanging-orchard'; accepted_parent_blob=$parentBlob;
  source_blob=$sourceBlob; mutation_seed='8a7c1940e6db2f53'; radius='high';
  frame=0; canonical=$true; near_threshold_inclusive=10000;
  requested=144; completed=$results.Count; failed=$failures.Count;
  unique_genomes=$uniqueGenomes; unique_pixels=$uniquePixels;
  near_representatives=$nearRepresentatives.Count; selected_count=$selected.Count;
  selected=@($selected | ForEach-Object {@{index=$_.descendant_index;candidate_identity=$_.candidate_identity;genome_identity=$_.genome_identity;pixel_identity=$_.pixel_identity;novelty_distance=$_.novelty_distance}});
  native_debug_release_replays=$replays; resumed_thread_count=1;
  manifest_sha256=$postResume;
  human_art_selection_pending=$true;
}
[IO.File]::WriteAllText((Join-Path $out 'audit.json'),(($audit | ConvertTo-Json -Depth 8)+[char]10),[Text.UTF8Encoding]::new($false))
Write-Host "PASS: $($results.Count) canonical indexed outputs, $uniquePixels unique pixels, $($selected.Count) diversity selections, $replays read-only native Debug/Release replays. Artistic selection pending."
