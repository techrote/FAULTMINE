param(
  [Parameter(Mandatory=$true)][string]$OutputDirectory,
  [ValidateRange(1,8)][int]$Threads=4,
  [switch]$Resume
)
$ErrorActionPreference='Stop'
$here=(Resolve-Path (Split-Path -Parent $MyInvocation.MyCommand.Path)).Path
$root=(Resolve-Path (Join-Path $here '../../..')).Path
$source=Join-Path $here 'source.bmp'
$parent=Join-Path $root 'examples/fm005-addressing-genome.json'
$batch=Join-Path $root 'build/Release/FAULTMINE-batch.exe'
$debug=Join-Path $root 'build/Debug/FAULTMINE-render.exe'
$release=Join-Path $root 'build/Release/FAULTMINE-render.exe'
$out=[IO.Path]::GetFullPath($OutputDirectory)
foreach($path in @($parent,$batch,$debug,$release)){
  if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing accepted input/executable: $path"}
}
if(-not(Test-Path -LiteralPath $source)) {
  & python (Join-Path $here 'make_source.py') --write
  if($LASTEXITCODE -ne 0){throw 'Cannot materialize original source'}
}
& python (Join-Path $here 'make_source.py') --verify
if($LASTEXITCODE -ne 0){throw 'Self-contained authored BMP does not match its generator'}
$srcHash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if($srcHash -cne 'c751316e7615d53cf8a43e79e3a8395ec63f49392e7d16e37be16ad5f100f875'){
  throw "Source drift: $srcHash"
}
$parentBlob=(& git -C $root hash-object -- $parent).Trim()
if($LASTEXITCODE -ne 0 -or $parentBlob -cne '64b0756262db458e276861dad84edbd964571558'){
  throw "Accepted parent Git blob has changed: $parentBlob"
}
if((Test-Path -LiteralPath $out) -and -not $Resume){
  throw "Output directory exists; only explicit -Resume permits reuse: $out"
}
if(-not(Test-Path -LiteralPath $out)){New-Item -ItemType Directory -Path $out -Force | Out-Null}
$common=@('--source',$source,'--genome',$parent,'--out',$out,'--seed','6ac9f2e430bd7185',
  '--begin','0','--count','128','--radius','high','--frame','0','--render','canonical',
  '--near-threshold','8192','--select','12','--selected-stills')
if($Resume){& $batch @common '--threads' "$Threads" '--resume'}
else{& $batch @common '--threads' "$Threads"}
$exit=$LASTEXITCODE;$global:LASTEXITCODE=0
if($exit -ne 0){throw "Native search failed, exit $exit; partial evidence retained in $out"}
$manifest=Join-Path $out 'batch.fmbatch.json'
if(-not(Test-Path -LiteralPath $manifest)){throw 'No authoritative native batch manifest'}
$m=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
$results=@($m.candidates);$failures=@($m.failures)
if($m.manifest_version -ne 1 -or $m.descriptor_version -ne 1 -or
   $m.mutation_policy_version -ne 1 -or
   $m.source_identity -cne '298051e0e8c048f36f4ac830baedf989e3daf4907fa360d75f59c8e3450dec88' -or
   $m.mutation_seed -cne '6ac9f2e430bd7185' -or $m.radius -cne 'high' -or
   $m.render_mode -cne 'canonical' -or $m.frame_index -ne 0 -or
   $m.index_begin -ne 0 -or $m.index_end_exclusive -ne 128 -or
   $m.near_duplicate_threshold -ne 8192 -or $m.requested_selection_count -ne 12 -or
   -not $m.complete -or $m.cancelled -or $results.Count -ne 128 -or $failures.Count -ne 0){
  throw 'Invalid, incomplete or wrong-address FM-014 manifest'
}
for($i=0;$i -lt 128;$i++){
  if([long]$results[$i].descendant_index -ne $i){throw "Out-of-order candidate $i"}
}
$uniqueGenomes=@($results | Select-Object -ExpandProperty genome_identity -Unique).Count
$uniquePixels=@($results | Select-Object -ExpandProperty pixel_identity -Unique).Count
$near=@($results | Where-Object {$_.near_representative_index -eq $_.descendant_index})
$selected=@($results | Where-Object {$_.selected})
if($uniqueGenomes -lt 24 -or $uniquePixels -lt 24 -or $near.Count -lt 6 -or $selected.Count -lt 6){
  throw "Insufficient diversity: genomes=$uniqueGenomes pixels=$uniquePixels near=$($near.Count) selected=$($selected.Count)"
}
foreach($f in @('batch.fmproj','selected-contact-sheet.png','selected-contact-sheet.png.fmmanifest.json')){
  if(-not(Test-Path -LiteralPath (Join-Path $out $f))){throw "Missing native export: $f"}
}
$before=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
& $batch @common '--threads' '1' '--resume'
$exit=$LASTEXITCODE;$global:LASTEXITCODE=0
if($exit -ne 0){throw "Verified 1-worker resume failed, exit $exit"}
$after=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
if($before -cne $after){throw 'Manifest changed across verified resume and thread count'}
$replays=0
foreach($c in $selected){
  $i=[long]$c.descendant_index
  $name=('candidate_{0:D20}.png' -f $i)
  $png=Join-Path (Join-Path $out 'selected') $name
  if(-not(Test-Path -LiteralPath $png) -or -not(Test-Path -LiteralPath ($png+'.fmmanifest.json'))){
    throw "Missing canonical selected PNG/sidecar: $i"
  }
  $gpath=Join-Path $out ('audit-genome-{0:D20}.json' -f $i)
  [IO.File]::WriteAllText($gpath,[string]$c.canonical_genome,[Text.UTF8Encoding]::new($false))
  $sha=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash
  foreach($exe in @($release,$debug)){
    $lines=@(& $exe '--verify' '--frame' '0' $source $gpath $png)
    $exit=$LASTEXITCODE;$global:LASTEXITCODE=0
    if($exit -ne 0 -or $lines -cnotcontains 'verification=passed' -or
       $lines -cnotcontains "source_identity=$($m.source_identity)" -or
       $lines -cnotcontains "output_identity=$($c.pixel_identity)"){
      throw "Decoded RGBA8 replay mismatch at descendant $i ($exe): $($lines -join '; ')"
    }
    $replays++
  }
  if((Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash -cne $sha){
    throw "Read-only verification overwrote selected evidence $i"
  }
}
$audit=[ordered]@{
  record_version=1; artwork='06-seismic-debt'; accepted_parent_blob=$parentBlob
  source_bmp_sha256=$srcHash; source_identity=$m.source_identity
  parent_genome_identity=$m.parent_genome_identity; mutation_seed='6ac9f2e430bd7185'
  radius='high'; frame=0; render_mode='canonical'; descriptor_version=1
  near_threshold_inclusive=8192; requested=128; completed=$results.Count; failed=$failures.Count
  unique_genomes=$uniqueGenomes; unique_pixels=$uniquePixels; near_representatives=$near.Count
  selected_count=$selected.Count; debug_release_readonly_replays=$replays
  verified_resume_manifest_sha256=$after; visual_selection_pending=$true
  selections=@($selected | ForEach-Object {
    @{index=$_.descendant_index;candidate_identity=$_.candidate_identity
      genome_identity=$_.genome_identity;pixel_identity=$_.pixel_identity
      novelty_distance=$_.novelty_distance}
  })
}
[IO.File]::WriteAllText((Join-Path $out 'audit.json'),(($audit | ConvertTo-Json -Depth 10)+[char]10),[Text.UTF8Encoding]::new($false))
Write-Host "CANONICAL: $($results.Count) complete; pixels=$uniquePixels, near=$($near.Count), selections=$($selected.Count), Debug/Release replays=$replays"
