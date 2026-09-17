param(
    [Parameter(Mandatory = $true)]
    [string]$ArchivePath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$absoluteArchive = if ([IO.Path]::IsPathRooted($ArchivePath)) { $ArchivePath } else { Join-Path $repoRoot $ArchivePath }
if (-not (Test-Path $absoluteArchive)) {
    throw "Portable archive was not found: $absoluteArchive"
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("FAULTMINE-package-check-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    Expand-Archive -Path $absoluteArchive -DestinationPath $tempRoot
    $packageDirs = @(Get-ChildItem -Path $tempRoot -Directory)
    if ($packageDirs.Count -ne 1) {
        throw "Archive must contain exactly one top-level package directory; found $($packageDirs.Count)."
    }
    $root = $packageDirs[0].FullName

    $required = @(
        'FAULTMINE.exe',
        'FAULTMINE-render.exe',
        'FAULTMINE-batch.exe',
        'FAULTMINE-lab.exe',
        'FAULTMINE-lab-worker.exe',
        'Run FAULTMINE.cmd',
        'PORTABLE.txt',
        'README.md',
        'docs\RAG_RELEASE_V1.md'
    )
    foreach ($relative in $required) {
        if (-not (Test-Path (Join-Path $root $relative))) {
            throw "Portable archive is missing required file: $relative"
        }
    }

    $forbiddenExtensions = @('.pdb', '.obj', '.lib', '.exp', '.ilk', '.cpp', '.c', '.hpp', '.h')
    $debris = @(Get-ChildItem -Path $root -Recurse -File | Where-Object {
        $forbiddenExtensions -contains $_.Extension.ToLowerInvariant() -or $_.Name -eq 'CMakeLists.txt'
    })
    if ($debris.Count -ne 0) {
        throw "Portable archive contains build/source debris: $($debris.FullName -join ', ')"
    }

    $mainExe = Join-Path $root 'FAULTMINE.exe'
    $versionInfo = (Get-Item $mainExe).VersionInfo
    if (-not $versionInfo.FileVersion.StartsWith('1.0.0') -or -not $versionInfo.ProductVersion.StartsWith('1.0.0')) {
        throw "FAULTMINE.exe version resource is inconsistent: file=$($versionInfo.FileVersion) product=$($versionInfo.ProductVersion)"
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'vswhere.exe is required by the CI package verifier to locate dumpbin.'
    }
    $installationPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    $dumpbin = Get-ChildItem -Path (Join-Path $installationPath 'VC\Tools\MSVC') -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x64\dumpbin.exe' } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
    if (-not $dumpbin) {
        throw 'Unable to locate x64 dumpbin.exe.'
    }
    foreach ($exe in @('FAULTMINE.exe', 'FAULTMINE-render.exe', 'FAULTMINE-batch.exe', 'FAULTMINE-lab.exe', 'FAULTMINE-lab-worker.exe')) {
        $dependencies = (& $dumpbin /dependents (Join-Path $root $exe)) -join "`n"
        if ($dependencies -match '(?im)^\s*(VCRUNTIME|MSVCP|CONCRT)[^\s]*\.dll\s*$') {
            throw "$exe unexpectedly depends on the dynamic MSVC runtime; v1 portable builds must use /MT."
        }
    }

    Push-Location $root
    try {
        $smoke = Start-Process -FilePath '.\FAULTMINE.exe' -ArgumentList '--smoke-test' -Wait -PassThru
        if ($smoke.ExitCode -ne 0) {
            throw "Packaged native window smoke failed with exit code $($smoke.ExitCode)."
        }
        $workflow = Start-Process -FilePath '.\FAULTMINE.exe' -ArgumentList '--release-self-test' -Wait -PassThru
        if ($workflow.ExitCode -ne 0) {
            throw "Packaged open/edit/lineage/save/reopen/export/temporal self-test failed with exit code $($workflow.ExitCode)."
        }
    }
    finally {
        Pop-Location
    }

    Write-Host "Portable package verification passed: $absoluteArchive"
}
finally {
    Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
