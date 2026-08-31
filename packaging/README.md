# Packaging

Each platform entry point performs a Release configure, build, test, install,
Qt runtime deployment, and archive operation:

- Windows x64 (Visual Studio 2026/v145):
  `powershell -File packaging/package-windows-x64.ps1`
- Linux x64: `bash packaging/package-linux-x64.sh`
- macOS ARM64: `bash packaging/package-macos-arm64.sh`

The Windows script discovers Visual Studio 2026 through `vswhere`, verifies the
v145 C++ toolset, and uses the CMake 4.3 installation bundled with Visual Studio
instead of an older CMake found on `PATH`. The Qt 6.10.3 paths match the
BigFileCleaner project and can be changed in the scripts or supplied through the
normal CMake cache. The archives are written inside their platform build
directories.
