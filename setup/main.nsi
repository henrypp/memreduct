; Includes
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "x64.nsh"

; Variables
Var StartMenuFolder
Var /Global Executable

; Defines
!define APP_NAME_LONG "Mem Reduct"
!define APP_NAME_SHORT "memreduct"
!define APP_AUTHOR "Henry++"
!define APP_VERSION "3.0.436"
!define APP_VERSION_FULL "3.0.436.0"
!define APP_WEBSITE "http://www.henrypp.org/product/${APP_NAME_SHORT}"
!define APP_FILES_DIR "bin"

!define COPYRIGHT "© 2015 Henry++. All Rights Reserved."
!define LICENSE_FILE "${APP_FILES_DIR}\License.txt"

!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_NODESC

; Pages
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION RunApplication
!define MUI_FINISHPAGE_LINK_LOCATION "${APP_WEBSITE}"
!define MUI_FINISHPAGE_LINK "${APP_WEBSITE}"
!define MUI_FINISHPAGE_TEXT_LARGE

!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
!insertmacro MUI_PAGE_COMPONENTS

!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM" 
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "StartMenuDir"
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

; Options
RequestExecutionLevel highest

Name "${APP_NAME_LONG}"
BrandingText "${COPYRIGHT}"

Caption "${APP_NAME_LONG} ${APP_VERSION}"
UninstallCaption "${APP_NAME_LONG} ${APP_VERSION}"

InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "InstallLocation"

OutFile "${APP_NAME_SHORT}_${APP_VERSION}_setup.exe"

AllowSkipFiles on
AutoCloseWindow false
XPStyle on

Icon "res\install.ico"
UninstallIcon "res\uninstall.ico"

Function .onInit
	${If} ${RunningX64}
		${If} $INSTDIR == ""
			StrCpy $INSTDIR "$PROGRAMFILES64\${APP_NAME_LONG}"
		${EndIf}

		StrCpy $Executable "${APP_NAME_SHORT}64.exe"
	${Else}
		${If} $INSTDIR == ""
			StrCpy $INSTDIR "$PROGRAMFILES32\${APP_NAME_LONG}"
		${EndIf}

		StrCpy $Executable "${APP_NAME_SHORT}.exe"
	${EndIf}
FunctionEnd

Function RunApplication
	Exec '"$INSTDIR\$Executable"'
FunctionEnd

Section "${APP_NAME_LONG}"
	SectionIn RO

	ExecWait '"cmd.exe" /c "taskkill.exe /im ${APP_NAME_SHORT}.exe && taskkill.exe /im ${APP_NAME_SHORT}64.exe"'

	SetOutPath $INSTDIR

	${If} ${RunningX64}
		File "${APP_FILES_DIR}\${APP_NAME_SHORT}64.exe"
	${Else}
		File "${APP_FILES_DIR}\${APP_NAME_SHORT}.exe"
	${EndIf}

	File "${APP_FILES_DIR}\History.txt"
	File "${APP_FILES_DIR}\License.txt"

	WriteUninstaller $INSTDIR\uninstall.exe

	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "DisplayName" "${APP_NAME_LONG}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "DisplayIcon" '"$INSTDIR\$Executable"'
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "DisplayVersion" "${APP_VERSION}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "InstallLocation" '"$INSTDIR"'
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "Publisher" "${APP_AUTHOR}"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "HelpLink" "${APP_WEBSITE}"
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}" "NoRepair" 1

	!insertmacro MUI_STARTMENU_WRITE_BEGIN Application

	CreateDirectory "$SMPROGRAMS\$StartMenuFolder"

	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\${APP_NAME_LONG}.lnk" "$INSTDIR\$Executable"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\License.lnk" "$INSTDIR\License.txt"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\History.lnk" "$INSTDIR\History.txt"
	CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\uninstall.exe"

	!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

Section "Create desktop shortcut"
	CreateShortCut "$DESKTOP\${APP_NAME_LONG}.lnk" "$INSTDIR\$Executable"
SectionEnd

Section /o "Store settings in application directory"
	SetOutPath $INSTDIR

	SetOverwrite off
	File "${APP_FILES_DIR}\${APP_NAME_SHORT}.ini"
	SetOverwrite on
SectionEnd

Section "Uninstall"
	; Clean install directory
	Delete "$INSTDIR\${APP_NAME_SHORT}.exe"
	Delete "$INSTDIR\${APP_NAME_SHORT}64.exe"
	Delete "$INSTDIR\${APP_NAME_SHORT}.ini"
	Delete "$INSTDIR\History.txt"
	Delete "$INSTDIR\License.txt"
	Delete "$INSTDIR\Uninstall.exe"

	; Delete shortcut's
	!insertmacro MUI_STARTMENU_GETFOLDER "Application" $StartMenuFolder

	RMDir /r "$SMPROGRAMS\$StartMenuFolder"
	Delete "$DESKTOP\${APP_NAME_LONG}.lnk"
	
	; Clean registry
	DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${APP_NAME_LONG}"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME_SHORT}"

	; Settings
	RMDir /r "$APPDATA\${APP_AUTHOR}\${APP_NAME_LONG}"
	RMDir "$APPDATA\${APP_AUTHOR}"

	RMDir "$INSTDIR"
SectionEnd

; Version info
VIAddVersionKey "ProductName" "${APP_NAME_LONG}"
VIAddVersionKey "Comments" "${APP_WEBSITE}"
VIAddVersionKey "FileDescription" "${APP_NAME_LONG}"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "LegalCopyright" "${COPYRIGHT}"
VIProductVersion "${APP_VERSION_FULL}"

!packhdr "$%TEMP%\exehead.tmp" '"upx.exe" "$%TEMP%\exehead.tmp"'