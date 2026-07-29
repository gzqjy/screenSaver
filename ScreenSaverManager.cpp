#include "ScreenSaverManager.h"
#include "IdleMonitor.h"
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

ScreenSaverManager::ScreenSaverManager(const ScreenSaverConfig &config, QObject *parent)
    : QObject(parent), m_config(config), m_active(false), m_activationTime(0)
{
    m_inputCheckTimer = new QTimer(this);
    m_inputCheckTimer->setInterval(100);
    connect(m_inputCheckTimer, &QTimer::timeout, this, &ScreenSaverManager::checkGlobalInput);
}

ScreenSaverManager::~ScreenSaverManager()
{
    deactivateAll();
}

void ScreenSaverManager::activateAll()
{
    if (m_active) return;
    m_active = true;
    m_activationTime = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "ScreenSaverManager: Activating on all screens";

    killLockApp();

    QList<QScreen*> screens = QGuiApplication::screens();
    qDebug() << "ScreenSaverManager: Detected" << screens.count() << "screens";
    for (QScreen *screen : screens) {
        qDebug() << "ScreenSaverManager: Screen geometry =" << screen->geometry();
        ScreenSaverWidget *w = new ScreenSaverWidget(m_config);
        connect(w, &ScreenSaverWidget::dismissed, this, &ScreenSaverManager::onWidgetDismissed);
        m_widgets.append(w);

        // 设置到对应的屏幕
        w->setGeometry(screen->geometry());
        w->activate();
        w->show();
    }

    // 启动全局硬件输入监控（绕过锁屏焦点的终极手段）
    m_inputCheckTimer->start();
}

void ScreenSaverManager::deactivateAll()
{
    IdleMonitor::reportActivity();
    if (!m_active) return;

    qDebug() << "ScreenSaverManager: Deactivating all screens";
    m_active = false;
    m_inputCheckTimer->stop();

    // 复制一份列表以安全删除
    QList<ScreenSaverWidget*> widgets = m_widgets;
    m_widgets.clear();

    for (ScreenSaverWidget *w : widgets) {
        w->deactivate();
        w->deleteLater();
    }

    emit allDismissed();
}

void ScreenSaverManager::onWidgetDismissed()
{
    deactivateAll();
}

void ScreenSaverManager::checkGlobalInput()
{
    if (!m_active) return;
    
    // 如果距启动不足 500 毫秒，忽略输入
    if (QDateTime::currentMSecsSinceEpoch() - m_activationTime < 500) {
        return;
    }

    // 获取系统硬件级空闲时间
    quint64 idleMs = IdleMonitor::getSystemIdleTimeMs();
    
    // 如果空闲时间突然小于 500 毫秒，说明刚刚有人动了鼠标或敲了键盘！
    if (idleMs < 500) {
        qDebug() << "ScreenSaverManager: Global input detected (idle =" << idleMs << "ms), deactivating";
        deactivateAll();
    }
}

void ScreenSaverManager::killLockApp()
{
#ifdef Q_OS_WIN
    // 在主屏幕启动时杀一次即可
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"LockApp.exe") == 0) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        TerminateProcess(hProc, 0);
                        CloseHandle(hProc);
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
#endif
}

