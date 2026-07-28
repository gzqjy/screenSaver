#ifndef _WIN32

#include "ScreenSaverDaemon.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

// ====================================================================
//  日志
// ====================================================================

static void logMsg(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[ScreenSaverDaemon] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(args);
}

// ====================================================================
//  辅助函数
// ====================================================================

static bool fileExists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

static bool fileReadable(const std::string &path)
{
    return access(path.c_str(), R_OK) == 0;
}

std::string ScreenSaverDaemon::readEnvFromProc(pid_t pid, const std::string &varName)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/environ", pid);

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
        return "";

    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());

    std::string prefix = varName + "=";
    size_t pos = 0;
    while (pos < content.size()) {
        size_t end = content.find('\0', pos);
        if (end == std::string::npos) end = content.size();

        std::string entry = content.substr(pos, end - pos);
        if (entry.compare(0, prefix.size(), prefix) == 0) {
            return entry.substr(prefix.size());
        }
        pos = end + 1;
    }
    return "";
}

pid_t ScreenSaverDaemon::findProcessByName(const std::string &name)
{
    DIR *dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;

        bool isNumeric = true;
        for (const char *p = entry->d_name; *p; ++p) {
            if (*p < '0' || *p > '9') { isNumeric = false; break; }
        }
        if (!isNumeric) continue;

        char commPath[64];
        snprintf(commPath, sizeof(commPath), "/proc/%s/comm", entry->d_name);

        std::ifstream ifs(commPath);
        if (ifs.is_open()) {
            std::string comm;
            std::getline(ifs, comm);
            while (!comm.empty() && (comm.back() == '\n' || comm.back() == '\r'))
                comm.pop_back();

            if (comm == name) {
                pid_t pid = static_cast<pid_t>(atoi(entry->d_name));
                closedir(dir);
                return pid;
            }
        }
    }
    closedir(dir);
    return -1;
}

// ====================================================================
//  X11 会话发现
// ====================================================================

std::string ScreenSaverDaemon::findXauthorityFile(const std::string &display)
{
    std::string displayNum = "0";
    if (!display.empty()) {
        size_t colonPos = display.find(':');
        if (colonPos != std::string::npos) {
            size_t dotPos = display.find('.', colonPos);
            if (dotPos != std::string::npos)
                displayNum = display.substr(colonPos + 1, dotPos - colonPos - 1);
            else
                displayNum = display.substr(colonPos + 1);
        }
    }

    // 常见路径
    std::vector<std::string> candidates = {
        "/var/run/lightdm/root/:" + displayNum,    // LightDM
        "/run/user/120/gdm/Xauthority",            // GDM (uid 120)
        "/run/user/121/gdm/Xauthority",            // GDM (uid 121)
        "/run/user/42/gdm/Xauthority",             // GDM (uid 42, Arch)
        "/root/.Xauthority",                        // root fallback
    };

    for (const auto &path : candidates) {
        if (fileReadable(path)) {
            logMsg("X11: found XAUTHORITY at: %s", path.c_str());
            return path;
        }
    }

    // 搜索 SDDM xauth 文件
    const char *searchDirs[] = { "/run/sddm", "/tmp", nullptr };
    for (int i = 0; searchDirs[i]; ++i) {
        DIR *dir = opendir(searchDirs[i]);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string fname = entry->d_name;
            if (fname.find("xauth") == 0 || fname.find(".serverauth") == 0) {
                std::string fullPath = std::string(searchDirs[i]) + "/" + fname;
                if (fileReadable(fullPath)) {
                    closedir(dir);
                    logMsg("X11: found XAUTHORITY at: %s", fullPath.c_str());
                    return fullPath;
                }
            }
        }
        closedir(dir);
    }

    return "";
}

bool ScreenSaverDaemon::findX11Session(DisplaySession &session)
{
    session.type = SessionType::X11;

    // 从 Xorg 进程获取信息
    pid_t xPid = findProcessByName("Xorg");
    if (xPid < 0)
        xPid = findProcessByName("X");

    if (xPid > 0) {
        logMsg("X11: found X server PID: %d", xPid);
        std::string envDisplay = readEnvFromProc(xPid, "DISPLAY");
        std::string envXauth   = readEnvFromProc(xPid, "XAUTHORITY");

        if (!envDisplay.empty()) session.display = envDisplay;
        if (!envXauth.empty() && fileReadable(envXauth))
            session.xauthority = envXauth;
    }

    // 从 lock 文件推断
    if (session.display.empty()) {
        DIR *dir = opendir("/tmp");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.size() > 7 && name.substr(0, 2) == ".X" &&
                    name.substr(name.size() - 5) == "-lock") {
                    std::string num = name.substr(2, name.size() - 7);
                    session.display = ":" + num;
                    break;
                }
            }
            closedir(dir);
        }
    }

    if (session.display.empty())
        session.display = ":0";

    // 从 DM 进程获取 XAUTHORITY
    if (session.xauthority.empty()) {
        const char *dmNames[] = { "lightdm", "gdm-session-wor", "gdm", "sddm", nullptr };
        for (int i = 0; dmNames[i]; ++i) {
            pid_t dmPid = findProcessByName(dmNames[i]);
            if (dmPid > 0) {
                std::string envXauth = readEnvFromProc(dmPid, "XAUTHORITY");
                if (!envXauth.empty() && fileReadable(envXauth)) {
                    session.xauthority = envXauth;
                    break;
                }
            }
        }
    }

    // 常见路径兜底
    if (session.xauthority.empty())
        session.xauthority = findXauthorityFile(session.display);

    return !session.display.empty();
}

// ====================================================================
//  Wayland 会话发现
// ====================================================================

bool ScreenSaverDaemon::findWaylandSession(DisplaySession &session)
{
    session.type = SessionType::Wayland;

    // 方法1: 从显示管理器进程获取 Wayland 环境
    const char *dmNames[] = {
        "gnome-shell",        // GDM 使用 gnome-shell 作为 Wayland 合成器
        "gsd-xsettings",      // GDM 辅助进程
        "gdm-wayland-ses",    // GDM Wayland session
        "kwin_wayland",       // SDDM + KDE
        "sddm-greeter",      // SDDM greeter
        "weston",             // Weston 合成器
        nullptr
    };

    for (int i = 0; dmNames[i]; ++i) {
        pid_t pid = findProcessByName(dmNames[i]);
        if (pid < 0) continue;

        logMsg("Wayland: found compositor '%s' PID: %d", dmNames[i], pid);

        std::string wayDisp = readEnvFromProc(pid, "WAYLAND_DISPLAY");
        std::string xdgDir  = readEnvFromProc(pid, "XDG_RUNTIME_DIR");

        if (!wayDisp.empty() && !xdgDir.empty()) {
            // 验证 socket 文件存在
            std::string socketPath = xdgDir + "/" + wayDisp;
            if (fileExists(socketPath)) {
                session.waylandDisplay = wayDisp;
                session.xdgRuntimeDir = xdgDir;
                logMsg("Wayland: WAYLAND_DISPLAY=%s, XDG_RUNTIME_DIR=%s",
                       wayDisp.c_str(), xdgDir.c_str());
                return true;
            }
        }

        // 有些合成器的 XDG_RUNTIME_DIR 在不同位置
        if (!wayDisp.empty()) {
            // 尝试常见的 runtime dir
            std::vector<std::string> runtimeDirs = {
                "/run/user/0",    // root
                "/run/user/120",  // gdm (Debian/Ubuntu)
                "/run/user/121",  // gdm
                "/run/user/42",   // gdm (Arch)
            };
            for (const auto &dir : runtimeDirs) {
                std::string socketPath = dir + "/" + wayDisp;
                if (fileExists(socketPath)) {
                    session.waylandDisplay = wayDisp;
                    session.xdgRuntimeDir = dir;
                    logMsg("Wayland: found socket at %s", socketPath.c_str());
                    return true;
                }
            }
        }
    }

    // 方法2: 搜索 wayland socket 文件
    std::vector<std::string> runtimeDirs = { "/run/user/0", "/run/user/120",
                                              "/run/user/121", "/run/user/42" };
    for (const auto &rtDir : runtimeDirs) {
        DIR *dir = opendir(rtDir.c_str());
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name.find("wayland-") == 0 && name.find(".lock") == std::string::npos) {
                session.waylandDisplay = name;
                session.xdgRuntimeDir = rtDir;
                closedir(dir);
                logMsg("Wayland: discovered socket %s/%s", rtDir.c_str(), name.c_str());
                return true;
            }
        }
        closedir(dir);
    }

    return false;
}

// ====================================================================
//  统一会话发现
// ====================================================================

bool ScreenSaverDaemon::findDisplaySession(DisplaySession &session)
{
    session.type = SessionType::Unknown;
    session.display.clear();
    session.xauthority.clear();
    session.waylandDisplay.clear();
    session.xdgRuntimeDir.clear();

    // 通过 loginctl 检测会话类型
    FILE *fp = popen("loginctl show-session $(loginctl list-sessions --no-legend | "
                     "head -1 | awk '{print $1}') -p Type --value 2>/dev/null", "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            std::string type = buf;
            while (!type.empty() && (type.back() == '\n' || type.back() == '\r'))
                type.pop_back();

            if (type == "wayland") {
                logMsg("session type detected: Wayland");
                pclose(fp);
                if (findWaylandSession(session)) return true;
                // Wayland 失败，可能有 XWayland 回退
                logMsg("Wayland discovery failed, trying X11/XWayland...");
                return findX11Session(session);
            }
        }
        pclose(fp);
    }

    // 检查是否有 Wayland 合成器运行
    if (findProcessByName("gnome-shell") > 0 || findProcessByName("kwin_wayland") > 0) {
        logMsg("detected Wayland compositor process");
        if (findWaylandSession(session)) return true;
    }

    // 检查是否有 X server 运行
    if (findProcessByName("Xorg") > 0 || findProcessByName("X") > 0) {
        logMsg("detected X server process");
        return findX11Session(session);
    }

    // 都试一遍
    logMsg("no session type hint, trying Wayland then X11...");
    if (findWaylandSession(session)) return true;
    return findX11Session(session);
}

// ====================================================================
//  用户登录检测
// ====================================================================

bool ScreenSaverDaemon::isUserLoggedIn()
{
    FILE *fp = popen("loginctl list-sessions --no-legend 2>/dev/null", "r");
    if (!fp) return false;

    char line[512];
    bool userSession = false;
    while (fgets(line, sizeof(line), fp)) {
        unsigned int uid = 0;
        char user[128] = {};
        if (sscanf(line, "%*s %u %127s", &uid, user) >= 2) {
            if (uid >= 1000) {
                userSession = true;
                break;
            }
        }
    }
    pclose(fp);
    return userSession;
}

// ====================================================================
//  启动 ScreenSaver 进程
// ====================================================================

pid_t ScreenSaverDaemon::launchScreenSaver(
    const std::string &exePath, const DisplaySession &session)
{
    pid_t pid = fork();

    if (pid < 0) {
        logMsg("fork() failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        // ===== 子进程 =====
        setsid();

        if (session.type == SessionType::Wayland) {
            // Wayland 环境变量
            setenv("WAYLAND_DISPLAY", session.waylandDisplay.c_str(), 1);
            setenv("XDG_RUNTIME_DIR", session.xdgRuntimeDir.c_str(), 1);
            setenv("QT_QPA_PLATFORM", "wayland", 1);
            // 设置 GDK 后端（如果 Qt 需要 GTK 主题支持）
            setenv("GDK_BACKEND", "wayland", 1);

            logMsg("child: Wayland mode, WAYLAND_DISPLAY=%s, XDG_RUNTIME_DIR=%s",
                   session.waylandDisplay.c_str(), session.xdgRuntimeDir.c_str());

        } else {
            // X11 环境变量
            setenv("DISPLAY", session.display.c_str(), 1);
            if (!session.xauthority.empty())
                setenv("XAUTHORITY", session.xauthority.c_str(), 1);
            setenv("QT_QPA_PLATFORM", "xcb", 1);

            logMsg("child: X11 mode, DISPLAY=%s", session.display.c_str());
        }

        execl(exePath.c_str(), exePath.c_str(), nullptr);
        logMsg("child: execl() failed: %s", strerror(errno));
        _exit(1);
    }

    logMsg("launched ScreenSaver PID: %d (%s)",
           pid, session.type == SessionType::Wayland ? "Wayland" : "X11");
    return pid;
}

// ====================================================================
//  进程存活检测
// ====================================================================

bool ScreenSaverDaemon::isProcessAlive(pid_t pid)
{
    if (pid <= 0) return false;
    if (kill(pid, 0) == 0) return true;
    int status;
    waitpid(pid, &status, WNOHANG);
    return false;
}

// ====================================================================
//  主运行循环
// ====================================================================

static volatile sig_atomic_t g_running = 1;

static void signalHandler(int /*sig*/)
{
    g_running = 0;
}

void ScreenSaverDaemon::run(const std::string &screenSaverPath)
{
    signal(SIGTERM, signalHandler);
    signal(SIGINT,  signalHandler);

    logMsg("daemon started, ScreenSaver path: %s", screenSaverPath.c_str());

    if (!fileExists(screenSaverPath)) {
        logMsg("ERROR: ScreenSaver not found: %s", screenSaverPath.c_str());
        return;
    }

    pid_t childPid = -1;
    const int CHECK_INTERVAL = 5;

    while (g_running) {
        sleep(CHECK_INTERVAL);
        if (!g_running) break;

        // 1. 有用户登录 → 方案 A
        if (isUserLoggedIn()) {
            if (childPid > 0 && isProcessAlive(childPid)) {
                logMsg("user logged in, stopping daemon-launched ScreenSaver (PID %d)", childPid);
                kill(childPid, SIGTERM);
                int status;
                waitpid(childPid, &status, 0);
                childPid = -1;
            }
            continue;
        }

        // 2. 子进程仍在运行
        if (childPid > 0 && isProcessAlive(childPid))
            continue;

        // 3. 回收退出的子进程
        if (childPid > 0) {
            int status;
            waitpid(childPid, &status, WNOHANG);
            childPid = -1;
        }

        // 4. 发现显示会话（自动检测 X11/Wayland）
        DisplaySession session;
        if (!findDisplaySession(session)) {
            logMsg("cannot find display session, retrying...");
            continue;
        }

        // 5. 启动 ScreenSaver
        logMsg("no user, launching ScreenSaver (%s)...",
               session.type == SessionType::Wayland ? "Wayland" : "X11");
        childPid = launchScreenSaver(screenSaverPath, session);

        if (childPid < 0)
            logMsg("launch failed, will retry");
    }

    // 清理
    if (childPid > 0 && isProcessAlive(childPid)) {
        kill(childPid, SIGTERM);
        sleep(2);
        if (isProcessAlive(childPid))
            kill(childPid, SIGKILL);
        int status;
        waitpid(childPid, &status, 0);
    }

    logMsg("daemon stopped");
}

// ====================================================================
//  systemd 安装/卸载
// ====================================================================

bool ScreenSaverDaemon::installSystemdService(const std::string &exePath)
{
    const std::string servicePath = "/etc/systemd/system/screensaver-guard.service";

    std::ostringstream oss;
    oss << "[Unit]\n"
        << "Description=Screen Saver Guard Daemon\n"
        << "After=display-manager.service\n"
        << "\n"
        << "[Service]\n"
        << "Type=simple\n"
        << "ExecStart=" << exePath << "\n"
        << "Restart=always\n"
        << "RestartSec=5\n"
        << "\n"
        << "[Install]\n"
        << "WantedBy=graphical.target\n";

    std::ofstream ofs(servicePath);
    if (!ofs.is_open()) {
        logMsg("cannot write to %s (need root)", servicePath.c_str());
        return false;
    }
    ofs << oss.str();
    ofs.close();

    system("systemctl daemon-reload");
    system("systemctl enable screensaver-guard.service");
    logMsg("systemd service installed: %s", servicePath.c_str());
    return true;
}

bool ScreenSaverDaemon::uninstallSystemdService()
{
    system("systemctl stop screensaver-guard.service 2>/dev/null");
    system("systemctl disable screensaver-guard.service 2>/dev/null");
    unlink("/etc/systemd/system/screensaver-guard.service");
    system("systemctl daemon-reload");
    logMsg("systemd service uninstalled");
    return true;
}

#endif // !_WIN32
