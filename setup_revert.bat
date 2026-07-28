@echo off
chcp 65001 >nul
echo ============================================
echo   还原自动登录配置
echo   需要以管理员权限运行
echo ============================================
echo.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 请右键 -> 以管理员身份运行此脚本！
    pause
    exit /b 1
)

echo [1/3] 关闭自动登录...
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v AutoAdminLogon /t REG_SZ /d "0" /f
reg delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon" /v DefaultPassword /f 2>nul

echo [2/3] 移除应用自启动...
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v ScreenSaver /f 2>nul

echo [3/3] 恢复系统锁屏...
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Windows\Personalization" /v NoLockScreen /f 2>nul

echo.
echo [完成] 所有配置已还原，重启后生效。
pause
