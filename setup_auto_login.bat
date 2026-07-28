@echo off
chcp 65001 >nul
echo ============================================
echo   Windows 自动登录 + 应用自启动 配置脚本
echo   需要以管理员权限运行
echo ============================================
echo.

:: ===== 检查管理员权限 =====
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 请右键 -> 以管理员身份运行此脚本！
    pause
    exit /b 1
)

:: ===== 配置参数（请根据实际情况修改） =====
set AUTO_LOGIN_USER=kiosk
set AUTO_LOGIN_PASS=your_password_here
set APP_PATH=C:\path\to\ScreenSaver.exe

:: ============================================
:: 第一步：配置 Windows 自动登录
:: ============================================
echo [1/3] 配置 Windows 自动登录...
echo   用户名: %AUTO_LOGIN_USER%

reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v AutoAdminLogon /t REG_SZ /d "1" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v DefaultUserName /t REG_SZ /d "%AUTO_LOGIN_USER%" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v DefaultPassword /t REG_SZ /d "%AUTO_LOGIN_PASS%" /f

echo   [完成] 自动登录已配置
echo.

:: ============================================
:: 第二步：配置应用开机自启动
:: ============================================
echo [2/3] 配置应用开机自启动...
echo   应用路径: %APP_PATH%

reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v ScreenSaver /t REG_SZ /d "\"%APP_PATH%\"" /f

echo   [完成] 自启动已配置
echo.

:: ============================================
:: 第三步：禁用 Windows 自带锁屏（可选）
:: ============================================
echo [3/3] 禁用 Windows 自带锁屏（避免与应用屏保冲突）...

reg add "HKLM\SOFTWARE\Policies\Microsoft\Windows\Personalization" /v NoLockScreen /t REG_DWORD /d 1 /f

echo   [完成] 系统锁屏已禁用
echo.

echo ============================================
echo   所有配置完成！重启后生效。
echo ============================================
echo.
echo   [提示] 如需还原，运行 setup_revert.bat
echo.
pause
