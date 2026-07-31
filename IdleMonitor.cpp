#include "IdleMonitor.h"
#include <QDebug>
#include <QLibrary>
#include <QCursor>
#include <QDateTime>

// ===== 平台相关：获取系统级空闲时间 =====
#ifdef Q_OS_WIN
  #include <windows.h>

#elif defined(Q_OS_LINUX)
  // D-Bus 用于 Wayland 空闲检测
  #include <QDBusConnection>
  #include <QDBusInterface>
  #include <QDBusReply>
  #include <QProcessEnvironment>

  // X11 用于 X11 空闲检测（可选，编译时检查）
  #if defined(HAVE_XSS)
    #include <X11/extensions/scrnsaver.h>
    #include <X11/Xlib.h>
  #endif
#endif

// ====================================================================
//  Linux: D-Bus 空闲时间查询（Wayland + X11 通用）
// ====================================================================

#ifdef Q_OS_LINUX

/// 方法1: GNOME Mutter IdleMonitor (Wayland + X11)
static qint64 getIdleTimeFromMutter()
{
    QDBusInterface iface(
        "org.gnome.Mutter.IdleMonitor",
        "/org/gnome/Mutter/IdleMonitor/Core",
        "org.gnome.Mutter.IdleMonitor",
        QDBusConnection::sessionBus());

    if (!iface.isValid())
        return -1;

    QDBusReply<quint64> reply = iface.call("GetIdletime");
    if (reply.isValid())
        return static_cast<qint64>(reply.value());

    return -1;
}

/// 方法2: KDE KIdleTime (Wayland + X11)
static qint64 getIdleTimeFromKDE()
{
    QDBusInterface iface(
        "org.kde.KIdleTime",
        "/org/kde/KIdleTime",
        "org.kde.KIdleTime",
        QDBusConnection::sessionBus());

    if (!iface.isValid())
        return -1;

    QDBusReply<int> reply = iface.call("idleTime");
    if (reply.isValid())
        return static_cast<qint64>(reply.value());

    return -1;
}

/// 方法3: freedesktop ScreenSaver (部分桌面环境支持)
static qint64 getIdleTimeFromFreedesktop()
{
    QDBusInterface iface(
        "org.freedesktop.ScreenSaver",
        "/org/freedesktop/ScreenSaver",
        "org.freedesktop.ScreenSaver",
        QDBusConnection::sessionBus());

    if (!iface.isValid())
        return -1;

    QDBusReply<quint32> reply = iface.call("GetSessionIdleTime");
    if (reply.isValid())
        return static_cast<qint64>(reply.value()) * 1000;  // 秒→毫秒

    return -1;
}

typedef struct {
    unsigned long window;
    int state;
    int kind;
    unsigned long til_or_since;
    unsigned long idle;
    unsigned long event_mask;
} XScreenSaverInfo_Dynamic;

static qint64 getIdleTimeFromX11Dynamic()
{
    static QLibrary xssLib("Xss", 1);
    if (!xssLib.isLoaded()) xssLib.load();
    if (!xssLib.isLoaded()) {
        xssLib.setFileName("Xss");
        xssLib.load();
    }

    if (xssLib.isLoaded()) {
        typedef XScreenSaverInfo_Dynamic* (*XScreenSaverAllocInfoFunc)();
        typedef int (*XScreenSaverQueryInfoFunc)(void*, unsigned long, XScreenSaverInfo_Dynamic*);
        
        static auto pAlloc = (XScreenSaverAllocInfoFunc)xssLib.resolve("XScreenSaverAllocInfo");
        static auto pQuery = (XScreenSaverQueryInfoFunc)xssLib.resolve("XScreenSaverQueryInfo");
        
        if (pAlloc && pQuery) {
            static QLibrary x11Lib("X11");
            if (!x11Lib.isLoaded()) x11Lib.load();
            if (x11Lib.isLoaded()) {
                typedef void* (*XOpenDisplayFunc)(const char*);
                typedef int (*XCloseDisplayFunc)(void*);
                typedef unsigned long (*XDefaultRootWindowFunc)(void*);
                typedef int (*XFreeFunc)(void*);
                
                static auto pOpen = (XOpenDisplayFunc)x11Lib.resolve("XOpenDisplay");
                static auto pClose = (XCloseDisplayFunc)x11Lib.resolve("XCloseDisplay");
                static auto pRoot = (XDefaultRootWindowFunc)x11Lib.resolve("XDefaultRootWindow");
                static auto pFree = (XFreeFunc)x11Lib.resolve("XFree");
                
                if (pOpen && pClose && pRoot && pFree) {
                    void* display = pOpen(nullptr);
                    if (display) {
                        XScreenSaverInfo_Dynamic* info = pAlloc();
                        qint64 idleMs = -1;
                        if (info) {
                            if (pQuery(display, pRoot(display), info)) {
                                idleMs = static_cast<qint64>(info->idle);
                            }
                            pFree(info);
                        }
                        pClose(display);
                        if (idleMs >= 0) return idleMs;
                    }
                }
            }
        }
    }
    return -1;
}

/// 方法4: X11 XScreenSaver 扩展
static qint64 getIdleTimeFromX11()
{
    qint64 dynamicIdle = getIdleTimeFromX11Dynamic();
    if (dynamicIdle >= 0) return dynamicIdle;

#if defined(HAVE_XSS)
    Display *display = XOpenDisplay(nullptr);
    if (!display)
        return -1;

    XScreenSaverInfo *info = XScreenSaverAllocInfo();
    if (!info) {
        XCloseDisplay(display);
        return -1;
    }

    qint64 idleMs = -1;
    if (XScreenSaverQueryInfo(display, DefaultRootWindow(display), info)) {
        idleMs = static_cast<qint64>(info->idle);
    }

    XFree(info);
    XCloseDisplay(display);
    return idleMs;
#else
    return -1;
#endif
}

/// 方法5: 读取 /proc/stat 输入设备中断计数（最终兜底）
/// 通过检测键盘/鼠标中断次数变化来推算空闲时间
static qint64 s_lastInterruptCount = -1;
static qint64 s_lastActivityTimestamp = 0;

static qint64 getIdleTimeFromProcInterrupts()
{
    // 读取 /proc/interrupts 中键盘(i8042)和鼠标相关中断
    FILE *fp = fopen("/proc/interrupts", "r");
    if (!fp) return -1;

    qint64 totalInterrupts = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // 匹配 i8042 (PS/2键盘鼠标) 或 xhci/ehci (USB)
        if (strstr(line, "i8042") || strstr(line, "xhci") || strstr(line, "ehci") || strstr(line, "uhci") || strstr(line, "ohci")) {
            // 解析中断计数（第一列之后的数字）
            char *p = line;
            while (*p && (*p < '0' || *p > '9')) p++;  // 跳过 IRQ 号和冒号
            // 累加各 CPU 上的中断数
            while (*p) {
                if (*p >= '0' && *p <= '9') {
                    totalInterrupts += strtoll(p, &p, 10);
                } else if (*p == ' ' || *p == '\t') {
                    p++;
                } else {
                    break;  // 到了设备名部分
                }
            }
        }
    }
    fclose(fp);

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (s_lastInterruptCount < 0) {
        // 首次调用
        s_lastActivityTimestamp = now - 1000000;
        return 1000000;
        return 0;
    }

    if (totalInterrupts != s_lastInterruptCount) {
        // 有新的输入中断 → 有用户活动
        s_lastInterruptCount = totalInterrupts;
        s_lastActivityTimestamp = now;
        return 0;
    }

    // 无新中断 → 返回距上次活动的时间
    return now - s_lastActivityTimestamp;
}

#endif // Q_OS_LINUX

// ====================================================================
//  getSystemIdleTimeMs: 跨平台统一接口
// ====================================================================

static qint64 s_manualActivityTime = 0;

void IdleMonitor::reportActivity()
{
    s_manualActivityTime = QDateTime::currentMSecsSinceEpoch();
}

static quint64 getSystemIdleTimeMsInternal()
{
#ifdef Q_OS_WIN
    // Windows: GetLastInputInfo (始终可用)
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (GetLastInputInfo(&lii)) {
        return static_cast<quint64>(GetTickCount()) - static_cast<quint64>(lii.dwTime);
    }
    return 0;

#elif defined(Q_OS_LINUX)
    // Linux: 按优先级尝试多种方法
    // 1. D-Bus: GNOME Mutter (Wayland + X11)
    qint64 idle = getIdleTimeFromMutter();
    if (idle >= 0) { /*qDebug() << "Mutter sysIdle:" << idle;*/ return static_cast<quint64>(idle); }

    // 2. D-Bus: KDE KIdleTime (Wayland + X11)
    idle = getIdleTimeFromKDE();
    if (idle >= 0) { /*qDebug() << "KDE sysIdle:" << idle;*/ return static_cast<quint64>(idle); }

    // 3. D-Bus: freedesktop ScreenSaver
    idle = getIdleTimeFromFreedesktop();
    if (idle >= 0) { /*qDebug() << "freedesktop sysIdle:" << idle;*/ return static_cast<quint64>(idle); }

    // 4. X11: XScreenSaver 扩展（仅 X11 会话）
    idle = getIdleTimeFromX11();
    if (idle >= 0) { /*qDebug() << "X11 sysIdle:" << idle;*/ return static_cast<quint64>(idle); }

    // 5. /proc/interrupts 兜底（不依赖任何显示服务）
    idle = getIdleTimeFromProcInterrupts();
    if (idle >= 0) { /*qDebug() << "Proc sysIdle:" << idle;*/ return static_cast<quint64>(idle); }

    return 0;

#else
    qWarning() << "IdleMonitor: unsupported platform";
    return 0;
#endif
}

// ===== IdleMonitor 实现 =====

IdleMonitor::IdleMonitor(QObject *parent)
    : QObject(parent)
    , m_idleTimeoutMs(300 * 1000)
    , m_loggedInIdleTimeoutMs(600 * 1000)
    , m_loggedIn(false)
    , m_active(false)
{
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(1000); // 每秒检查一次
    connect(m_checkTimer, &QTimer::timeout, this, &IdleMonitor::checkIdle);
}

void IdleMonitor::setIdleTimeout(int seconds)
{
    m_idleTimeoutMs = seconds * 1000;
}

void IdleMonitor::setLoggedInIdleTimeout(int seconds)
{
    m_loggedInIdleTimeoutMs = seconds * 1000;
}

void IdleMonitor::setLoggedIn(bool loggedIn)
{
    m_loggedIn = loggedIn;
    qDebug() << "IdleMonitor: login state =" << (loggedIn ? "logged in" : "not logged in")
             << ", timeout =" << currentTimeoutMs() / 1000 << "s";
}

void IdleMonitor::start()
{
    m_active = true;
    m_checkTimer->start();
    qDebug() << "IdleMonitor: started, current timeout =" << currentTimeoutMs() / 1000 << "s";
}

void IdleMonitor::stop()
{
    m_active = false;
    m_checkTimer->stop();
    qDebug() << "IdleMonitor: stopped";
}

int IdleMonitor::currentTimeoutMs() const
{
    return m_loggedIn ? m_loggedInIdleTimeoutMs : m_idleTimeoutMs;
}

void IdleMonitor::checkIdle()
{
    if (!m_active)
        return;

    quint64 idleMs = getSystemIdleTimeMs();

    int timeoutMs = currentTimeoutMs();

    if (static_cast<qint64>(idleMs) >= timeoutMs) {
        qDebug() << "IdleMonitor: system idle timeout reached ("
                 << idleMs / 1000 << "s >= " << timeoutMs / 1000 << "s)"
                 << (m_loggedIn ? "[logged in]" : "[not logged in]");
        stop();
        emit triggered();
    }
}

quint64 IdleMonitor::getSystemIdleTimeMs()
{
    quint64 sysIdle = getSystemIdleTimeMsInternal();
    // 跨平台终极后备：通过轮询全局鼠标位置检测鼠标活动
    static QPoint s_lastMousePos = QCursor::pos();
    QPoint currentMousePos = QCursor::pos();
    if ((currentMousePos - s_lastMousePos).manhattanLength() > 5) {
        s_lastMousePos = currentMousePos;
        reportActivity();
    }
#ifdef Q_OS_LINUX
    static QLibrary x11lib("X11");
    if (x11lib.load()) {
        typedef void* (*XOpenDisplayFunc)(const char*);
        typedef int (*XCloseDisplayFunc)(void*);
        typedef int (*XQueryKeymapFunc)(void*, char*);
        static XOpenDisplayFunc pXOpenDisplay = (XOpenDisplayFunc)x11lib.resolve("XOpenDisplay");
        static XCloseDisplayFunc pXCloseDisplay = (XCloseDisplayFunc)x11lib.resolve("XCloseDisplay");
        static XQueryKeymapFunc pXQueryKeymap = (XQueryKeymapFunc)x11lib.resolve("XQueryKeymap");
        if (pXOpenDisplay && pXCloseDisplay && pXQueryKeymap) {
            void* display = pXOpenDisplay(nullptr);
            if (display) {
                char currentKeymap[32];
                pXQueryKeymap(display, currentKeymap);
                pXCloseDisplay(display);
                static char s_lastKeymap[32] = {0};
                static bool s_initialized = false;
                if (!s_initialized) {
                    memcpy(s_lastKeymap, currentKeymap, 32);
                    s_initialized = true;
                } else {
                    if (memcmp(s_lastKeymap, currentKeymap, 32) != 0) {
                        memcpy(s_lastKeymap, currentKeymap, 32);
                        reportActivity();
                    }
                }
            }
        }
    }
#endif
    qint64 manualIdle = QDateTime::currentMSecsSinceEpoch() - s_manualActivityTime;
    if (manualIdle >= 0 && static_cast<quint64>(manualIdle) < sysIdle) {
        return static_cast<quint64>(manualIdle);
    }
    return sysIdle;
}
