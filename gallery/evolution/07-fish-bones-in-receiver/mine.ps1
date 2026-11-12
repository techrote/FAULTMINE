param(
  [Parameter(Mandatory=$true)][string]$OutputDirectory,
  [ValidateRange(1,8)][int]$Threads=4,
  [switch]$Resume
)
$ErrorActionPreference='Stop'
$here=(Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$root=(Resolve-Path (Join-Path $here '../../..')).Path
$source=Join-Path $here 'source.bmp'
$genome=Join-Path $here 'parent-genome.json'
$accepted=Join-Path $root 'examples/fm005-addressing-genome.json'
$batch=Join-Path $root 'build/Release/FAULTMINE-batch.exe'
$release=Join-Path $root 'build/Release/FAULTMINE-render.exe'
$debug=Join-Path $root 'build/Debug/FAULTMINE-render.exe'
$out=[IO.Path]::GetFullPath($OutputDirectory)
foreach($f in @($genome,$accepted,$batch,$debug,$release)) {
  if(-not(Test-Path -LiteralPath $f -PathType Leaf)) {throw "Missing accepted file or Windows native tool: $f"}
}
foreach($f in @($genome,$accepted)) {
  $blob=(& git -C $root hash-object -- $f).Trim()
  if($LASTEXITCODE -ne 0 -or $blob -cne '64b0756262db458e276861dad84edbd964571558') {
    throw "Parent genome drift: $f ($blob)"
  }
}
& python (Join-Path $here 'make_source.py') --write
if($LASTEXITCODE -ne 0){throw 'Cannot materialize deterministic authored source'}
& python (Join-Path $here 'make_source.py') --verify
if($LASTEXITCODE -ne 0){throw 'Authored BMP failed self-verification'}
& python (Join-Path $here 'preflight.py')
if($LASTEXITCODE -ne 0){throw 'Source/genome/identity preflight failed'}
$bmpHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if($bmpHash -cne 'ca6883cd3fc73526abdcba96671acf83f130005ed2b17d2f87e48c35b693f6cc'){
  throw "Authored source changed: $bmpHash"
}
if((Test-Path -LiteralPath $out) -and -not $Resume){
  throw "Destination already exists; pass -Resume to use verified native cache: $out"
}
if(-not(Test-Path -LiteralPath $out)) {New-Item -ItemType Directory $out | Out-Null}
$argsBase=@('--source',$source,'--genome',$genome,'--out',$out,
  '--seed','7e41a9c3d2b8065f','--begin','0','--count','96',
  '--threads',"$Threads",'--radius','high','--frame','0',
  '--render','canonical','--near-threshold','8192','--select','10','--selected-stills')
if($Resume){& $batch @argsBase '--resume'}else{& $batch @argsBase}
$exit=$LASTEXITCODE;$global:LASTEXITCODE=0
if($exit -ne 0){throw "Native batch failed ($exit); retain exact partial output: $out"}
$manifest=Join-Path $out 'batch.fmbatch.json'
if(-not(Test-Path -LiteralPath $manifest)){throw 'Batch manifest absent'}
$m=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
$results=@($m.candidates);$failures=@($m.failures)
if($m.manifest_version -ne 1 -or $m.descriptor_version -ne 1 -or
  $m.mutation_policy_version -ne 1 -or $m.source_identity -cne '76352a4b6b859a3605568e8b3a9a7f09caeab66bda1418c290d075d54026e6af' -or
  $m.mutation_seed -cne '7e41a9c3d2b8065f' -or $m.radius -cne 'high' -or
  $m.render_mode -cne 'canonical' -or $m.frame_index -ne 0 -or
  $m.index_begin -ne 0 -or $m.index_end_exclusive -ne 96 -or
  $m.near_duplicate_threshold -ne 8192 -or $m.requested_selection_count -ne 10 -or
  -not $m.complete -or $m.cancelled -or $results.Count -ne 96 -or $failures.Count -ne 0) {
  throw 'Batch did not complete the pinned canonical search address'
}
for($i=0;$i -lt 96;$i++){
  if([long]$results[$i].descendant_index -ne $i){throw "Candidate ordering mismatch at $i"}
}
$uniqueGenomes=@($results | Select-Object -ExpandProperty genome_identity -Unique).Count
$uniquePixels=@($results | Select-Object -ExpandProperty pixel_identity -Unique).Count
$near=@($results | Where-Object {$_.near_representative_index -eq $_.descendant_index})
$selected=@($results | Where-Object {$_.selected})
if($uniqueGenomes -lt 20 -or $uniquePixels -lt 20 -or $near.Count -lt 6 -or $selected.Count -lt 6){
  throw "Inadequate native diversity: genomes=$uniqueGenomes pixels=$uniquePixels near=$($near.Count) selected=$($selected.Count)"
}
foreach($c in $selected){
  if($c.near_representative_index -ne $c.descendant_index -or
     $c.pixel_representative_index -ne $c.descendant_index){
    throw "Native selection contains near or exact duplicate: $($c.descendant_index)"
  }
}
foreach($name in @('batch.fmproj','selected-contact-sheet.png','selected-contact-sheet.png.fmmanifest.json')){
  if(-not(Test-Path -LiteralPath (Join-Path $out $name))){throw "Missing native lineage or presentation export: $name"}
}
$before=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
$second=@('--source',$source,'--genome',$genome,'--out',$out,
  '--seed','7e41a9c3d2b8065f','--begin','0','--count','96',
  '--threads','1','--radius','high','--frame','0','--render','canonical',
  '--near-threshold','8192','--select','10','--selected-stills','--resume')
& $batch @second
$exit=$LASTEXITCODE;$global:LASTEXITCODE=0
if($exit -ne 0){throw "One-thread verified resume failed ($exit)"}
$after=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
if($before -cne $after){throw 'Native manifest changed under verified resume/worker count'}
$replays=0
foreach($c in $selected){
  $index=[long]$c.descendant_index
  $file=('candidate_{0:D20}.png' -f $index)
  $png=Join-Path (Join-Path $out 'selected') $file
  if(-not(Test-Path -LiteralPath $png) -or
     -not(Test-Path -LiteralPath ($png+'.fmmanifest.json'))){throw "Missing native full-res still: $index"}
  $genomePath=Join-Path $out ('audit-genome-{0:D20}.json' -f $index)
  [IO.File]::WriteAllText($genomePath,[string]$c.canonical_genome,[Text.UTF8Encoding]::new($false))
  $pngBefore=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash.ToLowerInvariant()
  foreach($exe in @($release,$debug)){
    $lines=@(& $exe '--verify' '--frame' '0' $source $genomePath $png)
    $status=$LASTEXITCODE;$global:LASTEXITCODE=0
    if($status -ne 0 -or $lines -cnotcontains 'verification=passed' -or
      $lines -cnotcontains "source_identity=$($m.source_identity)" -or
      $lines -cnotcontains "output_identity=$($c.pixel_identity)"){
      throw "Native decoded RGBA8 replay failed: descendant=$index; exe=$exe; $($lines -join '; ')"
    }
    $replays++
  }
  if((Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pngBefore){
    throw "Read-only verifier modified selected PNG $index"
  }
}
$baseline=Join-Path $out 'parent-baseline.png'
if(-not(Test-Path -LiteralPath $baseline)){
  $lines=@(& $release '--frame' '0' $source $genome $baseline)
  $status=$LASTEXITCODE;$global:LASTEXITCODE=0
  if($status -ne 0 -or -not(Test-Path -LiteralPath $baseline) -or
    $lines -cnotcontains 'source_identity=76352a4b6b859a3605568e8b3a9a7f09caeab66bda1418c290d075d54026e6af'){
    throw 'Native parent baseline render failed'
  }
}
$baseLines=@(& $debug '--verify' '--frame' '0' $source $genome $baseline)
$status=$LASTEXITCODE;$global:LASTEXITCODE=0
if($status -ne 0 -or $baseLines -cnotcontains 'verification=passed'){
  throw 'Debug independent parent baseline verification failed'
}
$audit=[ordered]@{
  record_version=1;artwork='07-fish-bones-in-receiver';native_executed=$true
  source_bmp_sha256=$bmpHash;source_identity=$m.source_identity
  accepted_parent_git_blob='64b0756262db458e276861dad84edbd964571558'
  parent_genome_identity=$m.parent_genome_identity
  mutation_seed='7e41a9c3d2b8065f';radius='high';frame=0
  render_mode='canonical';near_threshold_inclusive=8192;requested=96
  completed=$results.Count;failed=$failures.Count;unique_genomes=$uniqueGenomes
  unique_pixels=$uniquePixels;near_representatives=$near.Count
  selected_count=$selected.Count;debug_release_replays=$replays
  verified_resume_manifest_sha256=$after;human_visual_selection_pending=$true
  selections=@($selected | ForEach-Object {
    @{index=$_.descendant_index;candidate_identity=$_.candidate_identity
      genome_identity=$_.genome_identity;pixel_identity=$_.pixel_identity
      novelty_distance=$_.novelty_distance}
  })
}
[IO.File]::WriteAllText((Join-Path $out 'native-audit.json'),
  (($audit | ConvertTo-Json -Depth 10)+[char]10),[Text.UTF8Encoding]::new($false))
Write-Host "NATIVE COMPLETE: $($results.Count) descendants; $uniquePixels unique pixels; $($near.Count) near representatives; $($selected.Count) selected; $replays dual-configuration replays. Visual curation remains pending."
