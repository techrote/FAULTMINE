$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Installer's vswhere.exe was not found. Install Visual Studio/Build Tools with Desktop development with C++."
}

$installationVersion = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion | Select-Object -First 1)
if ([string]::IsNullOrWhiteSpace($installationVersion)) {
    throw 'No Visual Studio installation with the MSVC x64 toolchain was found.'
}

$visualStudioMajor = ([version]$installationVersion.Trim()).Major
$generator = switch ($visualStudioMajor) {
    18 { 'Visual Studio 18 2026'; break }
    17 { 'Visual Studio 17 2022'; break }
    default { throw "Unsupported Visual Studio major version $visualStudioMajor (installation $installationVersion)." }
}

$cmakeHelp = (& cmake --help) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "Unable to query CMake generators (exit code $LASTEXITCODE)."
}
if (-not $cmakeHelp.Contains($generator)) {
    throw "This CMake installation does not support generator '$generator'. Update CMake to a version that supports Visual Studio $visualStudioMajor."
}

Write-Host "Configuring FAULTMINE with $generator ($installationVersion), x64."

Push-Location $repoRoot
try {
    & cmake -S . -B build -G $generator -A x64 -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
