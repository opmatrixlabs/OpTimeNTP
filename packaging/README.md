# Packaging

Each platform entry point performs a Release configure, build, test, install,
and Qt runtime deployment:

- Windows x64 (Visual Studio 2026/v145):
  `powershell -File packaging/package-windows-x64.ps1`
- Linux x64: `bash packaging/package-linux-x64.sh`
- Linux ARM64 (Raspberry Pi 5): `bash packaging/package-linux-arm64.sh`
- macOS ARM64: `bash packaging/package-macos-arm64.sh`

The Linux scripts use architecture-native `linuxdeploy` and
`linuxdeploy-plugin-qt` executables from their Qt platform directories. The x64
script creates `cmake-build-release-wsl-linux-x86_64/OpTimeNTP.AppImage`; the
ARM64 script creates `cmake-build-release-wks-pi5/OpTimeNTP.AppImage` and must
run on an ARM64 host. Override tool locations with `OPTIME_QT_DIR`,
`LINUXDEPLOY_EXECUTABLE`, `LINUXDEPLOY_QT_PLUGIN`, or `LINUXDEPLOY_QMAKE` when
needed.

The macOS script uses `macdeployqt` and creates the self-contained bundle at
`cmake-build-macos-arm64-release/package/OpTimeNTP.app`. The application icon,
license, and third-party notices are embedded in the bundle's `Resources`
directory. Code signing and notarization are not performed.

The Windows script discovers Visual Studio 2026 through `vswhere`, verifies the
v145 C++ toolset, and uses the CMake 4.3 installation bundled with Visual Studio
instead of an older CMake found on `PATH`. The Qt 6.10.3 paths match the
BigFileCleaner project and can be changed in the scripts or supplied through the
normal CMake cache.

The Windows script also uses NSIS from `C:\Development\NSIS\makensis.exe` to
create `OpTimeNTP_<version>_setup.exe` inside the Windows build directory. Pass
`-NsisExecutable` to use another compiler location. The per-machine installer
installs to `C:\Program Files\OpTimeNTP`, creates an **OpTime NTP** Start menu
shortcut, offers an optional Desktop shortcut, and registers its uninstaller in
Windows Installed Apps. The installer, uninstaller, application executable, and
shortcuts use the multi-resolution icon generated from `resources/optime_ntp.svg`.
The portable Windows ZIP is also written inside the platform build directory
and is named
`OpTimeNTP_<version>_windows_x64.zip`.
