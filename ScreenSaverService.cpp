#ifdef _WIN32

#include "ScreenSaverService.h"
#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <cstdio>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")

// ===== 静态成员初始化 =====
const wchar_t* ScreenSaverService::SERVICE_NAME  = L"ScreenSaverGuard";
const wchar_t* ScreenSaverService::DISPLAY_NAME  = L"Screen Saver Guard Service";

SERVICE_STATUS        ScreenSaverService::s_status        = {};
SERVICE_STATUS_HANDLE ScreenSaverService::s_statusHandle  = nullptr;
HANDLE                ScreenSaverService::s_stopEvent     = nullptr;
HANDLE                ScreenSaverService::s_childProcess  = nullptr;
bool                  ScreenSaverService::s_isLocked      = false;
std::wstring          ScreenSaverService::s_screenSaverPath;

// ===== 日志输出 =====
void ScreenSaverService::logEvent(const wchar_t *msg)
{
    OutputDebugStringW(L"[ScreenSaverService] ");
    OutputDebugStringW(msg);
    OutputDebugStringW(L"\n");

    FILE *f = _wfopen(L"C:\\ScreenSaverService.log", L"a+");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, msg);
        fclose(f);
    }
}

// ===== 服务状态更新 =====
void ScreenSaverService::setServiceStatus(DWORD state, DWORD exitCode, DWORD waitHint)
{
    static DWORD checkPoint = 1;
    s_status.dwCurrentState  = state;
    s_status.dwWin32ExitCode = exitCode;
    s_status.dwWaitHint      = waitHint;

    if (state == SERVICE_START_PENDING)
        s_status.dwControlsAccepted = 0;
    else
        s_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;

    s_status.dwCheckPoint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : checkPoint++;

    SetServiceStatus(s_statusHandle, &s_status);
}

// ====================================================================
//  服务安装 / 卸载
// ====================================================================

bool ScreenSaverService::install(const std::wstring &exePath)
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) {
        logEvent(L"install: OpenSCManager failed");
        return false;
    }

    SC_HANDLE hService = CreateServiceW(
        hSCM,
        SERVICE_NAME,
        DISPLAY_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,          // 开机自动启动
        SERVICE_ERROR_NORMAL,
        exePath.c_str(),
        nullptr, nullptr, nullptr,
        nullptr,                     // LocalSystem 账户
        nullptr
    );

    if (!hService) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            logEvent(L"install: service already exists");
        } else {
            logEvent(L"install: CreateService failed");
        }
        CloseServiceHandle(hSCM);
        return false;
    }

    // 设置服务描述
    SERVICE_DESCRIPTIONW desc;
    desc.lpDescription = const_cast<LPWSTR>(
        L"守护 ScreenSaver.exe 进程，确保在任何会话状态下屏保正常运行");
    ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &desc);

    // 配置故障恢复：失败后自动重启
    SERVICE_FAILURE_ACTIONS_FLAG flag = { TRUE };
    ChangeServiceConfig2W(hService, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag);

    SC_ACTION actions[3] = {
        { SC_ACTION_RESTART, 5000 },  // 第1次失败：5秒后重启
        { SC_ACTION_RESTART, 10000 }, // 第2次失败：10秒后重启
        { SC_ACTION_RESTART, 30000 }, // 第3次失败：30秒后重启
    };
    SERVICE_FAILURE_ACTIONSW sfa = {};
    sfa.dwResetPeriod = 86400;  // 24小时重置失败计数
    sfa.cActions      = 3;
    sfa.lpsaActions   = actions;
    ChangeServiceConfig2W(hService, SERVICE_CONFIG_FAILURE_ACTIONS, &sfa);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    logEvent(L"install: service installed successfully");
    return true;
}

bool ScreenSaverService::uninstall()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return false;

    SC_HANDLE hService = OpenServiceW(hSCM, SERVICE_NAME, SERVICE_STOP | DELETE);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }

    // 先停止服务
    SERVICE_STATUS status;
    ControlService(hService, SERVICE_CONTROL_STOP, &status);
    Sleep(1000);

    BOOL result = DeleteService(hService);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);
    logEvent(result ? L"uninstall: success" : L"uninstall: failed");
    return result != FALSE;
}

// ====================================================================
//  服务运行
// ====================================================================

void ScreenSaverService::run()
{
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        { const_cast<LPWSTR>(SERVICE_NAME), serviceMain },
        { nullptr, nullptr }
    };
    StartServiceCtrlDispatcherW(serviceTable);
}

void WINAPI ScreenSaverService::serviceMain(DWORD /*argc*/, LPWSTR * /*argv*/)
{
    s_statusHandle = RegisterServiceCtrlHandlerExW(
        SERVICE_NAME, serviceCtrlHandlerEx, nullptr);
    if (!s_statusHandle) return;

    s_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    setServiceStatus(SERVICE_START_PENDING);

    s_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    // 计算 ScreenSaver.exe 路径（与服务 exe 同目录）
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    PathRemoveFileSpecW(modulePath);
    s_screenSaverPath = std::wstring(modulePath) + L"\\ScreenSaver.exe";

    setServiceStatus(SERVICE_RUNNING);
    logEvent(L"service started");

    // 进入工作循环
    serviceWorkerThread();

    // 清理
    if (s_childProcess) {
        TerminateProcess(s_childProcess, 0);
        CloseHandle(s_childProcess);
        s_childProcess = nullptr;
    }
    if (s_stopEvent) {
        CloseHandle(s_stopEvent);
        s_stopEvent = nullptr;
    }

    setServiceStatus(SERVICE_STOPPED);
    logEvent(L"service stopped");
}

DWORD WINAPI ScreenSaverService::serviceCtrlHandlerEx(
    DWORD ctrlCode, DWORD eventType, LPVOID eventData, LPVOID /*context*/)
{
    switch (ctrlCode) {
    case SERVICE_CONTROL_STOP:
        setServiceStatus(SERVICE_STOP_PENDING);
        SetEvent(s_stopEvent);
        break;

    case SERVICE_CONTROL_SESSIONCHANGE: {
        // 会话状态变化（用户登录/注销/锁屏/解锁等）
        WTSSESSION_NOTIFICATION *note =
            reinterpret_cast<WTSSESSION_NOTIFICATION*>(eventData);
        (void)note;

        switch (eventType) {
        case WTS_SESSION_LOGON:
            logEvent(L"session event: user logged on");
            // 用户登录了，终止旧的无用户进程，让方案 A 自启动接管
            if (s_childProcess) {
                TerminateProcess(s_childProcess, 0);
                CloseHandle(s_childProcess);
                s_childProcess = nullptr;
            }
            break;

        case WTS_SESSION_LOGOFF:
            logEvent(L"session event: user logged off");
            // 用户注销，需要重新在控制台会话启动屏保
            if (s_childProcess) {
                TerminateProcess(s_childProcess, 0);
                CloseHandle(s_childProcess);
                s_childProcess = nullptr;
            }
            break;

        case WTS_SESSION_LOCK:
            logEvent(L"session event: session locked");
            s_isLocked = true;
            // 锁屏时，需要在 Winlogon 桌面启动专门的后台监控进程
            if (s_childProcess) {
                TerminateProcess(s_childProcess, 0);
                CloseHandle(s_childProcess);
                s_childProcess = nullptr;
            }
            s_childProcess = launchInSession(note->dwSessionId, s_screenSaverPath, true);
            break;

        case WTS_SESSION_UNLOCK:
            logEvent(L"session event: session unlocked");
            s_isLocked = false;
            // 解锁后，清理 Winlogon 桌面上的屏保进程，由用户桌面进程接管
            if (s_childProcess) {
                TerminateProcess(s_childProcess, 0);
                CloseHandle(s_childProcess);
                s_childProcess = nullptr;
            }
            break;

        default:
            break;
        }
        break;
    }

    default:
        break;
    }
    
    return NO_ERROR;
}

// ====================================================================
//  工作线程：监控并维持 ScreenSaver.exe 运行
// ====================================================================

void ScreenSaverService::serviceWorkerThread()
{
    const DWORD CHECK_INTERVAL_MS = 5000;  // 每 5 秒检查一次

    while (WaitForSingleObject(s_stopEvent, CHECK_INTERVAL_MS) == WAIT_TIMEOUT) {

        // 1. 获取活动控制台会话
        DWORD sessionId = WTSGetActiveConsoleSessionId();
        if (sessionId == 0xFFFFFFFF) {
            continue;  // 没有活动控制台（例如远程桌面环境）
        }

        // 2. 检查是否有用户登录
        HANDLE hUserToken = nullptr;
        BOOL userLoggedIn = WTSQueryUserToken(sessionId, &hUserToken);
        if (hUserToken) {
            CloseHandle(hUserToken);
            hUserToken = nullptr;
        }

        if (userLoggedIn && !s_isLocked) {
            // 有用户登录且未锁屏
            if (isProcessRunningInSession(L"ScreenSaver.exe", sessionId)) {
                continue;  // 已在运行，无需操作
            }
            // 方案 A 未配置自启动时，服务补位启动
            if (!s_childProcess || WaitForSingleObject(s_childProcess, 0) == WAIT_OBJECT_0) {
                if (s_childProcess) {
                    CloseHandle(s_childProcess);
                    s_childProcess = nullptr;
                }
                logEvent(L"user logged in but ScreenSaver not running, launching...");
                s_childProcess = launchInSession(sessionId, s_screenSaverPath);
            }
        } else {
            // 无用户登录或已锁屏 → 方案 B：服务负责在 Winlogon 桌面上维持后台监控
            // 检查子进程是否存活
            if (s_childProcess && WaitForSingleObject(s_childProcess, 0) != WAIT_OBJECT_0) {
                continue;  // 子进程仍在运行
            }

            // 子进程不存在或已退出，重新启动
            if (s_childProcess) {
                CloseHandle(s_childProcess);
                s_childProcess = nullptr;
            }

            logEvent(L"no user or locked, launching ScreenSaver on Winlogon desktop...");
            s_childProcess = launchInSession(sessionId, s_screenSaverPath, true);

            if (s_childProcess) {
                logEvent(L"ScreenSaver launched successfully");
            } else {
                logEvent(L"failed to launch ScreenSaver");
            }
        }
    }
}

// ====================================================================
//  在指定会话中启动进程
// ====================================================================

HANDLE ScreenSaverService::launchInSession(DWORD sessionId, const std::wstring &exePath, bool forceWinlogon)
{
    HANDLE hToken = nullptr;

    // 尝试获取用户令牌（如果强制使用 Winlogon 则跳过）
    if (!forceWinlogon && WTSQueryUserToken(sessionId, &hToken)) {
        logEvent(L"launchInSession: using user token (default desktop)");
        HANDLE hProc = createProcessWithToken(hToken, exePath, L"winsta0\\default");
        CloseHandle(hToken);
        return hProc;
    }

    // 用户未登录 → 获取 winlogon.exe 的令牌
    logEvent(L"launchInSession: no user token, using winlogon token (Winlogon desktop)");
    hToken = getWinlogonToken(sessionId);
    if (!hToken) {
        logEvent(L"launchInSession: failed to get winlogon token");
        return nullptr;
    }

    HANDLE hProc = createProcessWithToken(hToken, exePath, L"winsta0\\Winlogon");
    CloseHandle(hToken);
    return hProc;
}

// ====================================================================
//  查找指定会话的 winlogon.exe 令牌
// ====================================================================

HANDLE ScreenSaverService::getWinlogonToken(DWORD sessionId)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return nullptr;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    HANDLE hToken = nullptr;

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"winlogon.exe") == 0) {
                // 检查是否在目标会话中
                DWORD procSessionId = 0;
                ProcessIdToSessionId(pe.th32ProcessID, &procSessionId);

                if (procSessionId == sessionId) {
                    HANDLE hProcess = OpenProcess(
                        PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
                    if (hProcess) {
                        HANDLE hProcToken = nullptr;
                        if (OpenProcessToken(hProcess,
                                TOKEN_DUPLICATE | TOKEN_QUERY |
                                TOKEN_ASSIGN_PRIMARY | TOKEN_IMPERSONATE,
                                &hProcToken)) {
                            // 复制为主令牌
                            if (!DuplicateTokenEx(hProcToken,
                                    MAXIMUM_ALLOWED, nullptr,
                                    SecurityImpersonation, TokenPrimary,
                                    &hToken)) {
                                hToken = nullptr;
                            }
                            CloseHandle(hProcToken);
                        }
                        CloseHandle(hProcess);
                    }
                    break;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return hToken;
}

// ====================================================================
//  使用令牌创建进程
// ====================================================================

HANDLE ScreenSaverService::createProcessWithToken(
    HANDLE hToken, const std::wstring &exePath, const std::wstring &desktop)
{
    // 复制令牌
    HANDLE hDupToken = nullptr;
    if (!DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, nullptr,
                          SecurityImpersonation, TokenPrimary, &hDupToken)) {
        logEvent(L"createProcess: DuplicateTokenEx failed");
        return nullptr;
    }

    // 黑科技：强制赋予令牌 UIAccess 权限，无需签名即可突破 LogonUI 的 Z-Band 限制
    DWORD dwUIAccess = 1;
    SetTokenInformation(hDupToken, TokenUIAccess, &dwUIAccess, sizeof(dwUIAccess));

    // 创建环境变量块
    LPVOID pEnv = nullptr;
    CreateEnvironmentBlock(&pEnv, hDupToken, FALSE);

    // 配置启动信息
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    // 设置目标桌面
    std::wstring desktopStr = desktop;
    si.lpDesktop = &desktopStr[0];

    PROCESS_INFORMATION pi = {};

    // 构建命令行 (启动后台监控)
    std::wstring cmdLine = L"\"" + exePath + L"\"";

    BOOL success = CreateProcessAsUserW(
        hDupToken,
        nullptr,              // lpApplicationName
        &cmdLine[0],          // lpCommandLine（可修改的缓冲区）
        nullptr,              // lpProcessAttributes
        nullptr,              // lpThreadAttributes
        FALSE,                // bInheritHandles
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE,
        pEnv,                 // lpEnvironment
        nullptr,              // lpCurrentDirectory
        &si,
        &pi
    );

    // 清理
    if (pEnv)
        DestroyEnvironmentBlock(pEnv);
    CloseHandle(hDupToken);

    if (!success) {
        wchar_t errMsg[128];
        swprintf(errMsg, 128, L"createProcess: CreateProcessAsUser failed, error=%lu",
                 GetLastError());
        logEvent(errMsg);
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;  // 返回进程句柄，用于监控
}

// ====================================================================
//  检查进程是否在指定会话中运行
// ====================================================================

bool ScreenSaverService::isProcessRunningInSession(
    const wchar_t *processName, DWORD sessionId)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    bool found = false;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                DWORD procSessionId = 0;
                ProcessIdToSessionId(pe.th32ProcessID, &procSessionId);
                if (procSessionId == sessionId) {
                    found = true;
                    break;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return found;
}

#endif // _WIN32
