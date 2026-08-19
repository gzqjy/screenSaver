; ScreenSaver Installer Script
!define PRODUCT_NAME "ScreenSaver"
!define PRODUCT_PUBLISHER "sinoparasoft"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; ------ MUI Definitions ------
!include "MUI.nsh"
!include "x64.nsh"
!include "WinVer.nsh" 

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\..\..\logo\logo.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

VIProductVersion "${MyVersion}"
VIAddVersionKey /LANG=${LANG_SimpChinese} "ProductName" "ScreenSaver"
VIAddVersionKey /LANG=${LANG_SimpChinese} "Comments" "ScreenSaver"
VIAddVersionKey /LANG=${LANG_SimpChinese} "CompanyName" "中科九洲科技股份有限公司"
VIAddVersionKey /LANG=${LANG_SimpChinese} "LegalCopyright" "Copyright (c) 2025 中科九洲科技股份有限公司"
VIAddVersionKey /LANG=${LANG_SimpChinese} "FileDescription" "屏幕保护服务"
VIAddVersionKey /LANG=${LANG_SimpChinese} "FileVersion" "${MyVersion}"
VIAddVersionKey /LANG=${LANG_SimpChinese} "ProductVersion" "${MyVersion}"

OutFile "ScreenSaver_Setup.exe"
InstallDir "$PROGRAMFILES\zkjsscreenSaver"
RequestExecutionLevel admin

Var CMDLINE_PARAMS

Section "Main" SEC01
	SetDetailsPrint both
	SectionIn RO
	SetOutPath $INSTDIR
	SetOverwrite on	
	
	File ".\HideWindow.dll"
	
	Call HookWindowVisible
		
	DetailPrint "停止旧服务..."
	ExecWait '"$INSTDIR\ScreenSaverService.exe" --stop'
	ExecWait '"$INSTDIR\ScreenSaverService.exe" --uninstall'
	
	DetailPrint "释放文件..."
	SetOutPath '$INSTDIR'
	File /r ".\bin\*.*"
	
	WriteUninstaller "$INSTDIR\uninst.exe"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${MyVersion}"
	WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
	
	Call setPermission
	
	DetailPrint "安装并启动服务..."
	ExecWait '"$INSTDIR\ScreenSaverService.exe" --install'
	ExecWait '"$INSTDIR\ScreenSaverService.exe" --start'

SectionEnd

Function .onInstSuccess
FunctionEnd

Section Uninstall
	SetRebootFlag true
	DetailPrint "卸载中..."

	SetOutPath $INSTDIR
	Call un.HookWindowVisible
	
	DetailPrint "停止并卸载服务..."
	ExecWait '"$INSTDIR\ScreenSaverService.exe" --stop'
	ExecWait '"$INSTDIR\ScreenSaverService.exe" --uninstall'
	
	DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
	RMDir /r /REBOOTOK "$INSTDIR\" 

	SetAutoClose true
SectionEnd

Function .onInit
	SetSilent silent
    StrCpy $CMDLINE_PARAMS $CMDLINE
	
	${WinVerGetMajor} $R0
    ${WinVerGetMinor} $R1

	${If} $R0 < 6
		MessageBox MB_OK "此程序仅支持 Windows 7 及更高版本系统。"
		Abort
	${ElseIf} $R0 == 6
		${If} $R1 < 1
		  MessageBox MB_OK "此程序仅支持 Windows 7 及更高版本系统。"
		  Abort
		${EndIf}
	${EndIf}
FunctionEnd

Function un.onInit
  SetShellVarContext all
FunctionEnd

Function HookWindowVisible
	DetailPrint "隐藏窗口..."
	SetOutPath $INSTDIR
	System::Call 'HideWindow.dll::Test(v)'
FunctionEnd

Function setPermission
	ExecWait 'cacls "$INSTDIR" /t /e /g administrators:f users:c'
FunctionEnd

Function un.HookWindowVisible
	DetailPrint "隐藏窗口..."
	SetOutPath $INSTDIR
	File ".\HideWindow.dll"
	System::Call 'HideWindow.dll::Test(v)'
FunctionEnd