#ifndef SCREENSAVERSERVICE_H
#define SCREENSAVERSERVICE_H

#ifdef _WIN32

#include <windows.h>
#include <string>

///
/// Windows 服务：守护 ScreenSaver.exe 进程
///
/// 职责：
///   1. 检测当前控制台会话状态（有用户/无用户登录）
///   2. 在活动会话中启动 ScreenSaver.exe
///   3. 监控进程存活，崩溃/退出后自动重启
///   4. 会话切换时（用户登录/注销）处理进程生命周期
///
/// 启动流程：
///   无用户登录 → 获取 winlogon.exe 令牌 → 在 Winlogon 桌面启动
///   有用户登录 → 获取用户令牌 → 在 Default 桌面启动
///
class ScreenSaverService
{
public:
    /// 服务名称
    static const wchar_t* SERVICE_NAME;
    static const wchar_t* DISPLAY_NAME;

    /// 安装服务
    static bool install(const std::wstring &exePath);
    /// 卸载服务
    static bool uninstall();
    /// 服务入口（由 SCM 调用）
    static void run();

    /// 在指定会话中启动 ScreenSaver.exe
    /// @param sessionId  目标会话 ID
    /// @param exePath    ScreenSaver.exe 完整路径
    /// @return 启动的进程句柄，失败返回 NULL
    static HANDLE launchInSession(DWORD sessionId, const std::wstring &exePath, bool forceWinlogon = false);

    /// 检查指定名称的进程是否在指定会话中运行
    static bool isProcessRunningInSession(const wchar_t *processName, DWORD sessionId);

    /// 查找指定会话中 winlogon.exe 的进程令牌
    static HANDLE getWinlogonToken(DWORD sessionId);

    /// 使用令牌在指定桌面创建进程
    static HANDLE createProcessWithToken(HANDLE hToken, const std::wstring &exePath,
                                         const std::wstring &desktop);

private:
    static void WINAPI serviceMain(DWORD argc, LPWSTR *argv);
    static DWORD WINAPI serviceCtrlHandlerEx(DWORD ctrlCode, DWORD eventType,
                                             LPVOID eventData, LPVOID context);
    static void serviceWorkerThread();
    static void setServiceStatus(DWORD state, DWORD exitCode = 0, DWORD waitHint = 0);
    static void logEvent(const wchar_t *msg);

    static SERVICE_STATUS        s_status;
    static SERVICE_STATUS_HANDLE s_statusHandle;
    static HANDLE                s_stopEvent;
    static HANDLE                s_childProcess;
    static bool                  s_isLocked;
    static std::wstring          s_screenSaverPath;
};

#endif // _WIN32
#endif // SCREENSAVERSERVICE_H
