#ifndef SCREENSAVERDAEMON_H
#define SCREENSAVERDAEMON_H

#ifndef _WIN32

#include <string>
#include <sys/types.h>

/// 显示会话类型
enum class SessionType {
    Unknown,
    X11,
    Wayland
};

/// 显示会话信息
struct DisplaySession {
    SessionType type;
    std::string display;        // X11: ":0"
    std::string xauthority;     // X11: XAUTHORITY 路径
    std::string waylandDisplay; // Wayland: "wayland-0"
    std::string xdgRuntimeDir; // Wayland: XDG_RUNTIME_DIR
    uid_t uid; // User ID owning the session
};

///
/// Linux 守护进程：在无用户登录时管理 ScreenSaver 进程
///
/// 支持：
///   - X11 会话 (LightDM / GDM X11 / SDDM X11)
///   - Wayland 会话 (GDM Wayland / SDDM Wayland)
///
class ScreenSaverDaemon
{
public:
    /// 主运行循环
    static void run(const std::string &screenSaverPath);

    /// 检测是否有用户图形会话登录
    static bool isUserLoggedIn();

    /// 自动发现活动显示会话（X11 或 Wayland）
    static bool getActiveTTYDisplay(DisplaySession &session, std::string &activeTty);

    /// 启动 ScreenSaver 进程
    static pid_t launchScreenSaver(const std::string &exePath,
                                   const DisplaySession &session);

    /// 检查进程是否存活
    static bool isProcessAlive(pid_t pid);

    /// 安装/卸载 systemd 服务
    static bool installSystemdService(const std::string &exePath);
    static bool uninstallSystemdService();

private:
    // ----- X11 发现 -----
    static bool findX11Session(DisplaySession &session);
    static std::string findXauthorityFile(const std::string &display);

    // ----- Wayland 发现 -----
    static bool findWaylandSession(DisplaySession &session);

    // ----- 通用辅助 -----
    static std::string readEnvFromProc(pid_t pid, const std::string &varName);
    static pid_t findProcessByName(const std::string &name);
    static std::string getAuthFromXorgCmdline(pid_t pid);
    static bool getActiveLoginctlSession(std::string &display, SessionType &type);
};

#endif // !_WIN32
#endif // SCREENSAVERDAEMON_H
