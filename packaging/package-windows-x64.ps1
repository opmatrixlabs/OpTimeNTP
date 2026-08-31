[CmdletBinding()]
param(
    [string] $BuildDir,
    [string] $Generator = "Visual Studio 18 2026",
    [string] $Toolset = "v145,host=x64",
    [string] $CMakeExecutable
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryDir = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repositoryDir "cmake-build-windows-vs2026-release"
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

$archivePath = Join-Path $BuildDir "OpTimeNTP_windows_x64.zip"
Compress-Archive -LiteralPath $stageDir -DestinationPath $archivePath `
    -CompressionLevel Optimal -Force
Write-Host "Created $archivePath"
