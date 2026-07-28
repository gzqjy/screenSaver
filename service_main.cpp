///
/// 屏保守护进程入口 - 跨平台
///
/// Windows:
///   ScreenSaverService.exe --install     安装 Windows 服务
///   ScreenSaverService.exe --uninstall   卸载 Windows 服务
///   ScreenSaverService.exe --start       启动服务
///   ScreenSaverService.exe --stop        停止服务
///   ScreenSaverService.exe               由 SCM 启动
///
/// Linux:
///   ScreenSaverDaemon --install          安装 systemd 服务
///   ScreenSaverDaemon --uninstall        卸载 systemd 服务
///   ScreenSaverDaemon                    前台运行（由 systemd 管理）
///   ScreenSaverDaemon --screensaver /path/to/ScreenSaver  指定屏保路径
///

#ifdef _WIN32
// ======================== Windows ========================

#include "ScreenSaverService.h"
#include <cstdio>
#include <string>
#include <clocale>

static void printUsage()
{
    wprintf(L"用法:\n");
    wprintf(L"  ScreenSaverService.exe --install     安装服务\n");
    wprintf(L"  ScreenSaverService.exe --uninstall   卸载服务\n");
    wprintf(L"  ScreenSaverService.exe --start       启动服务\n");
    wprintf(L"  ScreenSaverService.exe --stop        停止服务\n");
    wprintf(L"\n");
    wprintf(L"说明:\n");
    wprintf(L"  服务会自动在控制台会话中启动 ScreenSaver.exe\n");
    wprintf(L"  ScreenSaver.exe 需与本程序放在同一目录\n");
}

static bool startService()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) {
        wprintf(L"[错误] 无法连接服务管理器，请以管理员身份运行\n");
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCM, ScreenSaverService::SERVICE_NAME,
                                       SERVICE_START);
    if (!hService) {
        wprintf(L"[错误] 服务未安装，请先运行 --install\n");
        CloseServiceHandle(hSCM);
        return false;
    }

    BOOL ok = StartServiceW(hService, 0, nullptr);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    if (ok) {
        wprintf(L"[成功] 服务已启动\n");
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING)
            wprintf(L"[提示] 服务已在运行中\n");
        else
            wprintf(L"[错误] 启动失败，错误码: %lu\n", err);
    }
    return ok != FALSE;
}

static bool stopService()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;

    SC_HANDLE hService = OpenServiceW(hSCM, ScreenSaverService::SERVICE_NAME,
                                       SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!hService) {
        CloseServiceHandle(hSCM);
        return false;
    }

    SERVICE_STATUS status;
    BOOL ok = ControlService(hService, SERVICE_CONTROL_STOP, &status);
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    if (ok)
        wprintf(L"[成功] 服务已停止\n");
    else
        wprintf(L"[错误] 停止失败，错误码: %lu\n", GetLastError());

    return ok != FALSE;
}

int wmain(int argc, wchar_t *argv[])
{
    setlocale(LC_ALL, "");

    if (argc > 1) {
        std::wstring cmd = argv[1];

        if (cmd == L"--install" || cmd == L"-i") {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            if (ScreenSaverService::install(exePath)) {
                wprintf(L"[成功] 服务已安装\n");
                wprintf(L"  服务名称: %ls\n", ScreenSaverService::SERVICE_NAME);
                wprintf(L"  运行 --start 启动服务\n");
            } else {
                wprintf(L"[错误] 安装失败，请以管理员身份运行\n");
            }
            return 0;
        }

        if (cmd == L"--uninstall" || cmd == L"-u") {
            if (ScreenSaverService::uninstall())
                wprintf(L"[成功] 服务已卸载\n");
            else
                wprintf(L"[错误] 卸载失败\n");
            return 0;
        }

        if (cmd == L"--start")  { startService(); return 0; }
        if (cmd == L"--stop")   { stopService();  return 0; }

        if (cmd == L"--help" || cmd == L"-h") {
            printUsage();
            return 0;
        }

        wprintf(L"[错误] 未知参数: %ls\n\n", cmd.c_str());
        printUsage();
        return 1;
    }

    // 无参数 → 由 SCM 启动
    ScreenSaverService::run();
    return 0;
}

#else
// ======================== Linux ========================

#include "ScreenSaverDaemon.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <libgen.h>   // dirname
#include <unistd.h>
#include <linux/limits.h>

static void printUsage(const char *prog)
{
    printf("用法:\n");
    printf("  %s                                    前台运行\n", prog);
    printf("  %s --install                          安装 systemd 服务\n", prog);
    printf("  %s --uninstall                        卸载 systemd 服务\n", prog);
    printf("  %s --screensaver /path/to/ScreenSaver 指定屏保路径\n", prog);
    printf("\n");
    printf("说明:\n");
    printf("  守护进程会自动在 X 显示上启动 ScreenSaver\n");
    printf("  默认 ScreenSaver 与本程序在同一目录\n");
}

int main(int argc, char *argv[])
{
    // 获取自身路径
    char selfPath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
    if (len > 0) selfPath[len] = '\0';
    else         selfPath[0] = '\0';

    // 默认 ScreenSaver 在同一目录
    std::string selfDir = selfPath;
    size_t lastSlash = selfDir.rfind('/');
    if (lastSlash != std::string::npos)
        selfDir = selfDir.substr(0, lastSlash);
    std::string screenSaverPath = selfDir + "/ScreenSaver";

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--install") == 0 || strcmp(argv[i], "-i") == 0) {
            if (getuid() != 0) {
                printf("[错误] 安装需要 root 权限，请使用 sudo\n");
                return 1;
            }
            if (ScreenSaverDaemon::installSystemdService(selfPath)) {
                printf("[成功] systemd 服务已安装\n");
                printf("  启动: sudo systemctl start screensaver-guard\n");
                printf("  状态: sudo systemctl status screensaver-guard\n");
            } else {
                printf("[错误] 安装失败\n");
            }
            return 0;
        }

        if (strcmp(argv[i], "--uninstall") == 0 || strcmp(argv[i], "-u") == 0) {
            if (getuid() != 0) {
                printf("[错误] 卸载需要 root 权限\n");
                return 1;
            }
            if (ScreenSaverDaemon::uninstallSystemdService())
                printf("[成功] 服务已卸载\n");
            return 0;
        }

        if (strcmp(argv[i], "--screensaver") == 0 && i + 1 < argc) {
            screenSaverPath = argv[++i];
        }

        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }

    printf("[信息] ScreenSaver 路径: %s\n", screenSaverPath.c_str());

    // 进入守护循环
    ScreenSaverDaemon::run(screenSaverPath);
    return 0;
}

#endif
