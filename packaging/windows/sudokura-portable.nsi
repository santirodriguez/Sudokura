!ifndef VERSION
  !error "VERSION is required"
!endif
!ifndef ROOT
  !error "ROOT is required"
!endif
!ifndef DIST
  !error "DIST is required"
!endif
!ifndef OUTPUT
  !error "OUTPUT is required"
!endif

Unicode true
Name "Sudokura"
OutFile "${OUTPUT}"
Icon "${ROOT}\assets\generated\sudokura.ico"
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
ShowInstDetails nevershow
CRCCheck force
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "Sudokura"
VIAddVersionKey /LANG=1033 "FileDescription" "Sudokura portable launcher"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "CompanyName" "Santiago Rodriguez"
VIAddVersionKey /LANG=1033 "OriginalFilename" "Sudokura-v${VERSION}-windows-x86_64-portable.exe"

!include "FileFunc.nsh"

Section
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\Sudokura"
  File /r "${DIST}\*"

  ${GetParameters} $0

!ifdef SUDOKURA_PORTABLE_TEST_FAIL_LAUNCH
  Delete "$PLUGINSDIR\Sudokura\sudokura.exe"
!endif

  ClearErrors
  ExecWait '"$PLUGINSDIR\Sudokura\sudokura.exe" $0' $1
  IfErrors launch_failed launch_done

launch_failed:
  StrCpy $1 127

launch_done:
  SetOutPath "$TEMP"
  RMDir /r "$PLUGINSDIR\Sudokura"
  SetErrorLevel $1
SectionEnd
