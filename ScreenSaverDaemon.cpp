#ifndef _WIN32

#include "ScreenSaverDaemon.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <cstdarg>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

using namespace std;

static void logMsg(const char* format, ...) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", t);

    printf("[%s] [ScreenSaverDaemon] ", timeStr);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

static bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool ScreenSaverDaemon::getActiveTTYDisplay(DisplaySession &session, std::string &activeTty)
{
    FILE *fp = fopen("/sys/class/tty/tty0/active", "r");
    if (!fp) return false;
    char ttyBuf[32] = {0};
    if (!fgets(ttyBuf, sizeof(ttyBuf), fp)) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    
    for (int i=0; ttyBuf[i]; i++) {
        if (ttyBuf[i] == '\n' || ttyBuf[i] == '\r') ttyBuf[i] = 0;
    }
    
    activeTty = ttyBuf;
    std::string vtStr = "vt";
    if (activeTty.find("tty") == 0) {
        vtStr += activeTty.substr(3);
    }
    
    std::string cmd = "ps -eo pid,cmd | grep -E 'Xorg|Xwayland'";
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return false;
    
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), p)) {
        std::string s(line);
        if (s.find("grep") != std::string::npos) continue;
        
        if (s.find(vtStr) == std::string::npos && s.find(activeTty) == std::string::npos) {
            continue;
        }
        
        std::istringstream iss(s);
        std::string token;
        std::string display;
        std::string auth;
        
        while (iss >> token) {
            if (token[0] == ':' && token.length() >= 2 && isdigit(token[1])) {
                display = token;
            } else if (token == "-auth") {
                iss >> auth;
            }
        }
        
        if (!display.empty()) {
            session.type = SessionType::X11;
            session.display = display;
            session.xauthority = auth;
            found = true;
            break;
        }
    }
    pclose(p);
    return found;
}

pid_t ScreenSaverDaemon::launchScreenSaver(const std::string &exePath, const DisplaySession &session)
{
    pid_t pid = fork();
    if (pid < 0) {
        logMsg("fork() failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        setsid();
        if (session.type == SessionType::Wayland) {
            setenv("WAYLAND_DISPLAY", session.waylandDisplay.c_str(), 1);
            setenv("XDG_RUNTIME_DIR", session.xdgRuntimeDir.c_str(), 1);
            setenv("QT_QPA_PLATFORM", "wayland", 1);
            setenv("GDK_BACKEND", "wayland", 1);
        } else {
            setenv("DISPLAY", session.display.c_str(), 1);
            if (!session.xauthority.empty())
                setenv("XAUTHORITY", session.xauthority.c_str(), 1);
            setenv("QT_QPA_PLATFORM", "xcb", 1);
        }
        // Redirect stderr to log
        freopen("/tmp/screensaver_crash.log", "w", stderr);
        execl(exePath.c_str(), "ScreenSaver", nullptr);
        _exit(1);
    }
    return pid;
}

bool ScreenSaverDaemon::isProcessAlive(pid_t pid)
{
    if (pid <= 0) return false;
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) return true;
    return false;
}

static volatile sig_atomic_t g_running = 1;
static void signalHandler(int) { g_running = 0; }

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
    std::string currentActiveTty = "";
    const int CHECK_INTERVAL = 5;

    while (g_running) {
        sleep(CHECK_INTERVAL);
        if (!g_running) break;

        DisplaySession session;
        std::string activeTty;
        bool found = getActiveTTYDisplay(session, activeTty);
        
        if (found && activeTty != currentActiveTty) {
            logMsg("Active TTY changed from %s to %s", currentActiveTty.c_str(), activeTty.c_str());
            if (childPid > 0 && isProcessAlive(childPid)) {
                kill(childPid, SIGTERM);
                int status;
                waitpid(childPid, &status, 0);
            }
            childPid = -1;
            currentActiveTty = activeTty;
        }
        
        if (!found) continue;
        if (childPid > 0 && isProcessAlive(childPid)) continue;

        if (childPid > 0) {
            int status;
            waitpid(childPid, &status, WNOHANG);
            childPid = -1;
            logMsg("ScreenSaver dismissed by user on %s. Waiting 5 seconds...", activeTty.c_str());
            sleep(5);
            continue;
        }

        logMsg("Launching ScreenSaver on %s (hidden in background)...", activeTty.c_str());
        childPid = launchScreenSaver(screenSaverPath, session);
    }

    if (childPid > 0 && isProcessAlive(childPid)) {
        kill(childPid, SIGTERM);
        sleep(2);
        if (isProcessAlive(childPid)) kill(childPid, SIGKILL);
        int status;
        waitpid(childPid, &status, 0);
    }
    logMsg("daemon stopped");
}

bool ScreenSaverDaemon::installSystemdService(const std::string &exePath) {
    const std::string servicePath = "/etc/systemd/system/screensaver-guard.service";
    std::ofstream ofs(servicePath);
    if (!ofs.is_open()) {
        logMsg("ERROR: Failed to open %s for writing. Root privileges required?", servicePath.c_str());
        return false;
    }

    ofs << "[Unit]\n"
        << "Description=ScreenSaver Guard Daemon\n"
        << "After=display-manager.service\n\n"
        << "[Service]\n"
        << "Type=simple\n"
        << "ExecStart=" << exePath << "\n"
        << "Restart=always\n"
        << "RestartSec=3\n\n"
        << "[Install]\n"
        << "WantedBy=graphical.target\n"
        << "WantedBy=multi-user.target\n";
    ofs.close();

    logMsg("Reloading systemd daemon...");
    if (system("systemctl daemon-reload") != 0) {
        logMsg("Warning: systemctl daemon-reload failed");
    }

    logMsg("Enabling systemd service...");
    if (system("systemctl enable screensaver-guard.service") != 0) {
        logMsg("ERROR: Failed to enable screensaver-guard.service");
        return false;
    }

    return true;
}

bool ScreenSaverDaemon::uninstallSystemdService() {
    logMsg("Stopping systemd service...");
    if (system("systemctl stop screensaver-guard.service") != 0) {
        logMsg("Warning: systemctl stop failed");
    }

    logMsg("Disabling systemd service...");
    if (system("systemctl disable screensaver-guard.service") != 0) {
        logMsg("Warning: systemctl disable failed");
    }

    const std::string servicePath = "/etc/systemd/system/screensaver-guard.service";
    if (remove(servicePath.c_str()) != 0) {
        logMsg("Warning: Failed to remove %s", servicePath.c_str());
    } else {
        logMsg("Removed %s", servicePath.c_str());
    }

    logMsg("Reloading systemd daemon...");
    if (system("systemctl daemon-reload") != 0) {
        logMsg("Warning: systemctl daemon-reload failed");
    }

    return true;
}

#endif // !_WIN32
