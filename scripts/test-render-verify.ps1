param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$BuildDir = 'build'
)

$ErrorActionPreference = 'Stop'
# Expected negative native exits are checked explicitly; they are not test failures.
$PSNativeCommandUseErrorActionPreference = $false

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderExecutable = Join-Path $repoRoot "$BuildDir/$Configuration/FAULTMINE-render.exe"
if (-not (Test-Path $renderExecutable)) {
    throw "Missing native renderer: $renderExecutable"
}

function Invoke-RenderExpectedExit {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ExpectedCode,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )
    $lines = @(& $renderExecutable @Arguments)
    $actualCode = $LASTEXITCODE
    # Actions pwsh wrapper exits with LASTEXITCODE after script completion.
    # Expected nonzero negative cases must not poison a successful test run.
    $global:LASTEXITCODE = 0
    if ($actualCode -ne $ExpectedCode) {
        throw "$Label returned $actualCode; expected $ExpectedCode."
    }
    return $lines
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("FAULTMINE-render-verify-" + [guid]::NewGuid().ToString('N'))
New-Item -Path $tempRoot -ItemType Directory -Force | Out-Null
try {
    $source = Join-Path $tempRoot 'source.bmp'
    $different = Join-Path $tempRoot 'different.bmp'
    $genome = Join-Path $tempRoot 'canonical-genome.json'
    $badGenome = Join-Path $tempRoot 'bad-genome.json'
    $temporalGenome = Join-Path $tempRoot 'temporal-genome.json'
    $rendered = Join-Path $tempRoot 'rendered.png'
    $frameTwo = Join-Path $tempRoot 'frame-2.png'
    $temporalZero = Join-Path $tempRoot 'temporal-0.png'
    $temporalTwo = Join-Path $tempRoot 'temporal-2.png'

    # Tiny 2x2 24-bit BMP with four distinct pixels, no external assets.
    $sourceBytes = [Convert]::FromBase64String(
        'Qk1GAAAAAAAAADYAAAAoAAAAAgAAAAIAAAABABgAAAAAABAAAAATCwAAEwsAAAAAAAAAAAAA/wAAAP8AAAAAAP////8AAA==')
    [IO.File]::WriteAllBytes($source, $sourceBytes)
    $differentBytes = [byte[]]::new($sourceBytes.Length)
    [Array]::Copy($sourceBytes, $differentBytes, $sourceBytes.Length)
    $differentBytes[54] = 0
    [IO.File]::WriteAllBytes($different, $differentBytes)

    $utf8 = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText(
        $genome,
        '{"schema_version":1,"engine_contract_version":1,"root_seed":"0123456789abcdef","operators":[]}' + "`n",
        $utf8)
    [IO.File]::WriteAllText(
        $badGenome,
        '{"schema_version":2,"engine_contract_version":1,"root_seed":"0123456789abcdef","operators":[]}' + "`n",
        $utf8)
    # Ramp displacement is -1, 0, +1 at frames 0, 1, 2.
    # With fill boundaries, the asymmetric 2x2 source has distinct results.
    $temporalGenomeText = '{"schema_version":1,"engine_contract_version":1,"root_seed":"0123456789abcdef","operators":[{"instance_id":"00112233445566778899aabbccddeeff","type_id":"temporal.phase-drift","type_version":1,"enabled":true,"parameters":{"shape":{"kind":"string","value":"ramp"},"period_frames":{"kind":"u64","value":3},"phase_frames":{"kind":"u64","value":0},"amplitude_pixels":{"kind":"u64","value":1},"boundary":{"kind":"string","value":"fill"}}}]}'
    [IO.File]::WriteAllText($temporalGenome, $temporalGenomeText + "`n", $utf8)

    $normal = @(Invoke-RenderExpectedExit 0 @($source, $genome, $rendered) 'frame-zero render')
    if (-not (Test-Path $rendered) -or
        @($normal | Where-Object { $_ -match '^source_identity=[0-9a-f]{64}$' }).Count -ne 1 -or
        @($normal | Where-Object { $_ -match '^output_identity=[0-9a-f]{64}$' }).Count -ne 1) {
        throw 'Normal render did not produce a PNG and canonical identity records.'
    }

    $beforeVerify = (Get-FileHash -Algorithm SHA256 -Path $rendered).Hash
    $verified = @(Invoke-RenderExpectedExit 0 @('--verify', $source, $genome, $rendered) 'frame-zero read-only verification')
    if ($verified -notcontains 'verification=passed' -or $verified -notcontains 'frame=0') {
        throw 'Read-only verification did not report a passing frame.'
    }
    if ((Get-FileHash -Algorithm SHA256 -Path $rendered).Hash -ne $beforeVerify) {
        throw 'Read-only verification modified the expected image.'
    }

    $explicit = @(Invoke-RenderExpectedExit 0 @('--frame', '2', $source, $genome, $frameTwo) 'explicit frame render')
    if ($explicit -notcontains 'frame=2') {
        throw 'Explicit-frame render did not report frame=2.'
    }
    $explicitVerify = @(Invoke-RenderExpectedExit 0 @('--verify', '--frame', '2', $source, $genome, $frameTwo) 'explicit frame verification')
    if ($explicitVerify -notcontains 'verification=passed' -or $explicitVerify -notcontains 'frame=2') {
        throw 'Explicit frame replay verification did not pass.'
    }

    $temporalAtZero = @(Invoke-RenderExpectedExit 0 @('--frame', '0', $source, $temporalGenome, $temporalZero) 'temporal frame zero')
    $temporalAtTwo = @(Invoke-RenderExpectedExit 0 @('--frame', '2', $source, $temporalGenome, $temporalTwo) 'temporal frame two')
    $zeroIdentity = @($temporalAtZero | Where-Object { $_ -match '^output_identity=[0-9a-f]{64}$' })
    $twoIdentity = @($temporalAtTwo | Where-Object { $_ -match '^output_identity=[0-9a-f]{64}$' })
    if ($zeroIdentity.Count -ne 1 -or $twoIdentity.Count -ne 1 -or $zeroIdentity[0] -eq $twoIdentity[0]) {
        throw 'Distinct temporal frames did not produce distinct canonical identities.'
    }
    $temporalVerified = @(Invoke-RenderExpectedExit 0 @('--verify', '--frame', '2', $source, $temporalGenome, $temporalTwo) 'temporal frame two verification')
    if ($temporalVerified -notcontains 'verification=passed' -or $temporalVerified -notcontains 'frame=2') {
        throw 'Actual temporal operator replay verification failed.'
    }

    # Negative admissions: mismatch, missing file, wrong version or invalid frame.
    Invoke-RenderExpectedExit 10 @('--verify', $source, $genome, $different) 'pixel mismatch' | Out-Null
    Invoke-RenderExpectedExit 10 @('--verify', '--frame', '0', $source, $temporalGenome, $temporalTwo) 'cross-frame temporal mismatch' | Out-Null
    Invoke-RenderExpectedExit 9 @('--verify', $source, $genome, (Join-Path $tempRoot 'absent.png')) 'missing reference' | Out-Null
    Invoke-RenderExpectedExit 5 @('--verify', $source, $badGenome, $rendered) 'unsupported schema' | Out-Null
    Invoke-RenderExpectedExit 2 @('--frame', '-1', $source, $genome, $rendered) 'negative frame rejection' | Out-Null
    Invoke-RenderExpectedExit 2 @('--frame', '18446744073709551616', $source, $genome, $rendered) 'overflow frame rejection' | Out-Null

    if ((Get-FileHash -Algorithm SHA256 -Path $rendered).Hash -ne $beforeVerify) {
        throw 'Negative verification modified the canonical image.'
    }
    Write-Host "Canonical renderer CLI acceptance PASS ($Configuration): exact pixel/frame replay, negative admissions, read-only output."
}
finally {
    Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
