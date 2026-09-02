Unicode true

!include "LogicLib.nsh"
!define MUI_ICON "..\resources\optime_ntp.ico"
!define MUI_UNICON "..\resources\optime_ntp.ico"
!include "MUI2.nsh"
!include "x64.nsh"

!ifndef APP_VERSION
    !error "APP_VERSION must be provided by package-windows-x64.ps1."
!endif
!ifndef APP_VERSION_QUAD
    !error "APP_VERSION_QUAD must be provided by package-windows-x64.ps1."
!endif
!ifndef STAGE_DIR
    !error "STAGE_DIR must be provided by package-windows-x64.ps1."
!endif
!ifndef OUTPUT_FILE
    !error "OUTPUT_FILE must be provided by package-windows-x64.ps1."
!endif

!define PRODUCT_NAME "OpTime NTP"
!define PRODUCT_EXE "OpTimeNTP.exe"
!define PRODUCT_PUBLISHER "OpMatrix Inc."
!define PRODUCT_WEB_SITE "https://github.com/opmatrixsoftware/OpTimeNTP"
!define PRODUCT_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpTimeNTP"

Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\OpTimeNTP"
InstallDirRegKey HKLM "${PRODUCT_UNINSTALL_KEY}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "${APP_VERSION_QUAD}"
VIAddVersionKey /LANG=1033 "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=1033 "FileDescription" "${PRODUCT_NAME} Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (c) ${PRODUCT_PUBLISHER}"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\${PRODUCT_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${PRODUCT_NAME}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE_DIR}\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "${PRODUCT_NAME} requires 64-bit Windows."
        Abort
    ${EndIf}
    SetShellVarContext all
    SetRegView 64
FunctionEnd

Section "${PRODUCT_NAME} application files" ApplicationFiles
    SectionIn RO
    SetShellVarContext all
    SetRegView 64
    SetOutPath "$INSTDIR"
    File /r "${STAGE_DIR}\*"

    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}.lnk" \
        "$INSTDIR\bin\${PRODUCT_EXE}" "" \
        "$INSTDIR\bin\${PRODUCT_EXE}" 0 SW_SHOWNORMAL "" \
        "${PRODUCT_NAME}"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\bin\${PRODUCT_EXE}"
    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
    WriteRegStr HKLM "${PRODUCT_UNINSTALL_KEY}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
    WriteRegDWORD HKLM "${PRODUCT_UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${PRODUCT_UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section /o "Desktop shortcut" DesktopShortcut
    SetShellVarContext all
    CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" \
        "$INSTDIR\bin\${PRODUCT_EXE}" "" \
        "$INSTDIR\bin\${PRODUCT_EXE}" 0 SW_SHOWNORMAL "" \
        "${PRODUCT_NAME}"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${ApplicationFiles} \
        "Install ${PRODUCT_NAME} and create a Start menu shortcut."
    !insertmacro MUI_DESCRIPTION_TEXT ${DesktopShortcut} \
        "Create a shortcut to ${PRODUCT_NAME} on the desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
    SetShellVarContext all
    SetRegView 64

    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME}.lnk"
    DeleteRegKey HKLM "${PRODUCT_UNINSTALL_KEY}"

    RMDir /r "$INSTDIR"
SectionEnd
