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
  if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing native executable/parent: $path"}
}
if(-not(Get-Command python -ErrorAction SilentlyContinue)){throw 'Python 3 required only for materializing deterministic authored source'}
if(-not(Test-Path -LiteralPath $source)){
  & python (Join-Path $here 'make_source.py') '--write'
  if($LASTEXITCODE -ne 0){throw 'Deterministic source materialization failed'}
}
& python (Join-Path $here 'make_source.py') '--verify'
if($LASTEXITCODE -ne 0){throw 'Deterministic source reproducibility check failed'}
$actualSource=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
if($actualSource -cne '4d62b0c7e04f169110928e602af16b58370c0c26eddd313962beadaf02d1c59c'){
  throw "Authored source fingerprint mismatch: $actualSource"
}
if(-not(Get-Command git -ErrorAction SilentlyContinue)){throw 'Git needed for accepted parent pin'}
$parentBlob=(& git -C $root hash-object -- $parent).Trim()
if($LASTEXITCODE -ne 0 -or $parentBlob -cne '64b0756262db458e276861dad84edbd964571558'){
  throw "Immutable accepted parent changed: $parentBlob"
}
if((Test-Path -LiteralPath $out) -and -not $Resume){
  throw "Output already exists; use explicit -Resume for safe verified cache revalidation: $out"
}
if(-not(Test-Path -LiteralPath $out)){New-Item -Path $out -ItemType Directory -Force | Out-Null}
$common=@('--source',$source,'--genome',$parent,'--out',$out,
  '--seed','f70ab531c9d204e6','--begin','0','--count','96',
  '--radius','high','--frame','0','--render','canonical',
  '--near-threshold','8192','--select','8','--selected-stills')
if($Resume){& $batch @common '--threads' "$Threads" '--resume'}
else{& $batch @common '--threads' "$Threads"}
$exit=$LASTEXITCODE; $global:LASTEXITCODE=0
if($exit -ne 0){throw "Native canonical search failed ($exit); evidence retained in $out"}
$manifest=Join-Path $out 'batch.fmbatch.json'
if(-not(Test-Path -LiteralPath $manifest)){throw 'Missing authoritative native batch manifest'}
$m=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
$results=@($m.candidates);$failures=@($m.failures)
if($m.manifest_version -ne 1 -or $m.descriptor_version -ne 1 -or
   $m.mutation_policy_version -ne 1 -or $m.source_identity -cne '31c65bdb62b16ce1c890ac5f4beeca7910dc26e5ad2509b6ca186d1faa34ca1b' -or
   $m.mutation_seed -cne 'f70ab531c9d204e6' -or $m.radius -cne 'high' -or
   $m.render_mode -cne 'canonical' -or $m.frame_index -ne 0 -or
   $m.index_begin -ne 0 -or $m.index_end_exclusive -ne 96 -or
   $m.near_duplicate_threshold -ne 8192 -or $m.requested_selection_count -ne 8 -or
   -not $m.complete -or $m.cancelled -or $results.Count -ne 96 -or $failures.Count -ne 0){
   throw 'Incomplete native mine or incorrect immutable FM-014 search address'
}
for($i=0;$i -lt 96;$i++){
  if([long]$results[$i].descendant_index -ne $i){throw "Noncanonical index order at $i"}
}
$uniqueGenomes=@($results | Select-Object -ExpandProperty genome_identity -Unique).Count
$uniquePixels=@($results | Select-Object -ExpandProperty pixel_identity -Unique).Count
$representatives=@($results | Where-Object {$_.near_representative_index -eq $_.descendant_index})
$selected=@($results | Where-Object {$_.selected})
if($uniqueGenomes -lt 20 -or $uniquePixels -lt 20 -or $representatives.Count -lt 6 -or $selected.Count -lt 6){
  throw "Insufficient measurable diversity: genomes=$uniqueGenomes pixels=$uniquePixels near=$($representatives.Count) selected=$($selected.Count)"
}
foreach($f in @('batch.fmproj','selected-contact-sheet.png','selected-contact-sheet.png.json')){
  if(-not(Test-Path -LiteralPath (Join-Path $out $f))){throw "Missing native output: $f"}
}
$before=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
& $batch @common '--threads' '1' '--resume'
$exit=$LASTEXITCODE;$global:LASTEXITCODE=0
if($exit -ne 0){throw "Verified single-worker resume failed ($exit)"}
$after=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash.ToLowerInvariant()
if($before -cne $after){throw 'Semantic manifest changed across worker count/resume'}
$replays=0
foreach($c in $selected){
  $i=[long]$c.descendant_index
  $file=('candidate_{0:D20}.png' -f $i)
  $png=Join-Path (Join-Path $out 'selected') $file
  if(-not(Test-Path -LiteralPath $png) -or -not(Test-Path -LiteralPath ($png+'.fmmanifest.json'))){
    throw "Selected canonical PNG or FM-012 provenance missing: $i"
  }
  $genomePath=Join-Path $out ('audit-genome-{0:D20}.json' -f $i)
  [IO.File]::WriteAllText($genomePath,[string]$c.canonical_genome,[Text.UTF8Encoding]::new($false))
  $pre=(Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash
  foreach($exe in @($release,$debug)){
    $lines=@(& $exe '--verify' '--frame' '0' $source $genomePath $png)
    $exit=$LASTEXITCODE; $global:LASTEXITCODE=0
    if($exit -ne 0 -or $lines -cnotcontains 'verification=passed' -or
      $lines -cnotcontains "output_identity=$($c.pixel_identity)" -or
      $lines -cnotcontains "source_identity=$($m.source_identity)"){
      throw "Read-only Debug/Release replay mismatch: index=$i exe=$exe :: $($lines -join ';')"
    }
    $replays++
  }
  if((Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash -cne $pre){
    throw "Read-only replay modified selected PNG $i"
  }
}
$audit=[ordered]@{
  record_version=1; artwork='05-shearwing-telemetry'; source_sha256=$actualSource
  source_identity=$m.source_identity; accepted_parent_git_blob=$parentBlob
  accepted_parent_genome_identity=$m.parent_genome_identity
  mutation_seed='f70ab531c9d204e6'; radius='high'; frame=0
  canonical=$true; near_threshold_inclusive=8192; requested=96
  completed=$results.Count; failed=$failures.Count; unique_genomes=$uniqueGenomes
  unique_pixels=$uniquePixels; near_representatives=$representatives.Count
  selected_count=$selected.Count; debug_release_readonly_replays=$replays
  one_thread_resume_manifest_sha256=$after; aesthetic_selection_pending=$true
  selections=@($selected | ForEach-Object {
    @{index=$_.descendant_index;candidate_identity=$_.candidate_identity
    genome_identity=$_.genome_identity;pixel_identity=$_.pixel_identity
    novelty_distance=$_.novelty_distance}
  })
}
[IO.File]::WriteAllText((Join-Path $out 'audit.json'),(($audit | ConvertTo-Json -Depth 8)+[char]10),[Text.UTF8Encoding]::new($false))
Write-Host "PASS: $($results.Count) real canonical candidates, $uniquePixels unique pixels, $($selected.Count) selected, $replays native Debug/Release replays."
