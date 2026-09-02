[CmdletBinding()]
param(
    [string] $BuildDir,
    [string] $Generator = "Visual Studio 18 2026",
    [string] $Toolset = "v145,host=x64",
    [string] $CMakeExecutable,
    [string] $NsisExecutable = "C:\Development\NSIS\makensis.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryDir = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repositoryDir "cmake-build-release-visualstudio-2026"
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$repositoryPrefix = $repositoryDir.TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar
if (-not $BuildDir.StartsWith($repositoryPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    -not (Split-Path -Leaf $BuildDir).StartsWith("cmake-build-", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDir must be a cmake-build-* directory inside the repository."
}

$vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
    throw "Visual Studio Installer's vswhere.exe was not found."
}

$vsInstallations = @(& $vswherePath -latest -products "*" -version "[18.0,19.0)" `
    -requires "Microsoft.VisualStudio.Component.VC.Tools.x86.x64" `
    -property installationPath)
if ($LASTEXITCODE -ne 0 -or $vsInstallations.Count -eq 0) {
    throw "Visual Studio 2026 with the Desktop development with C++ workload was not found."
}
$vsInstallationPath = $vsInstallations[0].Trim()
$env:VCINSTALLDIR = Join-Path $vsInstallationPath "VC\"

$msvcToolsDir = Join-Path $vsInstallationPath "VC\Tools\MSVC"
$v145Installations = @(Get-ChildItem -LiteralPath $msvcToolsDir -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name.StartsWith("14.5", [System.StringComparison]::Ordinal) })
if ($v145Installations.Count -eq 0) {
    throw "The Visual Studio 2026 v145 MSVC toolset was not found."
}

if ([string]::IsNullOrWhiteSpace($CMakeExecutable)) {
    $CMakeExecutable = Join-Path $vsInstallationPath `
        "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
} elseif (Test-Path -LiteralPath $CMakeExecutable -PathType Leaf) {
    $CMakeExecutable = [System.IO.Path]::GetFullPath($CMakeExecutable)
} else {
    $cmakeCommand = Get-Command $CMakeExecutable -ErrorAction SilentlyContinue
    if ($null -eq $cmakeCommand) {
        throw "CMake executable '$CMakeExecutable' was not found."
    }
    $CMakeExecutable = $cmakeCommand.Source
}
if (-not (Test-Path -LiteralPath $CMakeExecutable -PathType Leaf)) {
    throw "A Visual Studio 2026-capable CMake executable was not found."
}

$CTestExecutable = Join-Path (Split-Path -Parent $CMakeExecutable) "ctest.exe"
if (-not (Test-Path -LiteralPath $CTestExecutable -PathType Leaf)) {
    throw "ctest.exe was not found beside '$CMakeExecutable'."
}

if (-not (Test-Path -LiteralPath $NsisExecutable -PathType Leaf)) {
    throw "NSIS compiler '$NsisExecutable' was not found. Install NSIS or pass -NsisExecutable."
}
$NsisExecutable = [System.IO.Path]::GetFullPath($NsisExecutable)

$cmakeHelp = (& $CMakeExecutable --help 2>&1) -join [System.Environment]::NewLine
if ($LASTEXITCODE -ne 0 -or
    $cmakeHelp.IndexOf($Generator, [System.StringComparison]::Ordinal) -lt 0) {
    throw "CMake '$CMakeExecutable' does not support the '$Generator' generator."
}

& $CMakeExecutable -S $repositoryDir -B $BuildDir -G $Generator -A x64 -T $Toolset `
    "-DCMAKE_GENERATOR_INSTANCE:PATH=$vsInstallationPath" `
    -DCMAKE_PREFIX_PATH=C:/Development/Qt/6.10.3/msvc2022_64 `
    -DOPTIME_DEPLOY_QT_RUNTIME=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

$cmakeCachePath = Join-Path $BuildDir "CMakeCache.txt"
$cmakeCache = Get-Content -LiteralPath $cmakeCachePath -Raw
$versionMatch = [regex]::Match(
    $cmakeCache,
    '(?m)^CMAKE_PROJECT_VERSION:STATIC=([0-9]+(?:\.[0-9]+){1,3})\s*$'
)
if (-not $versionMatch.Success) {
    throw "Could not read the OpTimeNTP project version from '$cmakeCachePath'."
}
$appVersion = $versionMatch.Groups[1].Value
$versionParts = [System.Collections.Generic.List[string]]::new()
$versionParts.AddRange([string[]] $appVersion.Split('.'))
while ($versionParts.Count -lt 4) {
    $versionParts.Add('0')
}
foreach ($versionPart in $versionParts) {
    $numericVersionPart = 0
    if (-not [int]::TryParse($versionPart, [ref] $numericVersionPart) -or
        $numericVersionPart -gt 65535) {
        throw "Version '$appVersion' cannot be used as Windows version metadata."
    }
}
$windowsVersion = $versionParts -join '.'

& $CMakeExecutable --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Release build failed." }

& $CTestExecutable --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed." }

$packageParent = Join-Path $BuildDir "package"
$stageDir = Join-Path $packageParent "OpTimeNTP"
if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

& $CMakeExecutable --install $BuildDir --config Release --prefix $stageDir
if ($LASTEXITCODE -ne 0) { throw "Install/deployment failed." }

$releaseDir = Join-Path $BuildDir "Release"
$stageBinDir = Join-Path $stageDir "bin"
foreach ($runtimeFile in @("dxcompiler.dll", "dxil.dll")) {
    $runtimeSource = Join-Path $releaseDir $runtimeFile
    if (Test-Path -LiteralPath $runtimeSource -PathType Leaf) {
        Copy-Item -LiteralPath $runtimeSource -Destination $stageBinDir -Force
    }
}

$archivePath = Join-Path $BuildDir "OpTimeNTP_${appVersion}_windows_x64.zip"
Compress-Archive -LiteralPath $stageDir -DestinationPath $archivePath `
    -CompressionLevel Optimal -Force
Write-Host "Created $archivePath"

$installerScript = Join-Path $PSScriptRoot "OpTimeNTP.nsi"
$installerPath = Join-Path $BuildDir "OpTimeNTP_${appVersion}_setup.exe"
& $NsisExecutable /V3 `
    "/DAPP_VERSION=$appVersion" `
    "/DAPP_VERSION_QUAD=$windowsVersion" `
    "/DSTAGE_DIR=$stageDir" `
    "/DOUTPUT_FILE=$installerPath" `
    $installerScript
if ($LASTEXITCODE -ne 0) { throw "NSIS installer creation failed." }
if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf)) {
    throw "NSIS did not create the expected installer '$installerPath'."
}
Write-Host "Created $installerPath"
