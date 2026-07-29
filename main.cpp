#include <QApplication>
#include <QFileInfo>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFontDatabase>
#include <QStringList>
#include <QDirIterator>
#include "ScreenSaverConfig.h"
#include "IdleMonitor.h"
#include "ScreenSaverManager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 日志文件处理器
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString txt;
    switch (type) {
    case QtDebugMsg:    txt = QString("Debug: %1").arg(msg); break;
    case QtWarningMsg:  txt = QString("Warning: %1").arg(msg); break;
    case QtCriticalMsg: txt = QString("Critical: %1").arg(msg); break;
    case QtFatalMsg:    txt = QString("Fatal: %1").arg(msg); break;
    case QtInfoMsg:     txt = QString("Info: %1").arg(msg); break;
    }
    
    QString logPath = QCoreApplication::applicationDirPath() + "/ScreenSaver_Exe_Debug.log";
    QFile outFile(logPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream ts(&outFile);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") 
           << txt << endl;
        outFile.close();
    }
    
    if (type == QtFatalMsg) abort();
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(customMessageHandler);
    QApplication app(argc, argv);
    app.setApplicationName("ScreenSaver");
    
    // 关键修正：防止屏保窗口关闭时整个后台检测进程自动退出！
    app.setQuitOnLastWindowClosed(false);

    // ---------- 解析命令行参数 ----------
    bool immediateShow = false;
    QString configPath = "screensaver_config.json";

    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--show" || arg.toLower() == "-s" || arg.toLower() == "/s") {
            immediateShow = true;          // 立即显示屏保
        } else if (arg.toLower() == "/p") {
            // 预览模式：Windows 屏保设置界面的小窗口预览（暂且直接退出，或者也可以实现）
            return 0;
        } else if (arg.toLower() == "/c") {
            // 配置模式：Windows 屏保设置（暂且退出，后续可做配置界面）
            return 0;
        } else if (arg == "--config" || arg == "-c") {
            if (i + 1 < argc)
                configPath = QString::fromLocal8Bit(argv[++i]);
        } else if (!arg.startsWith("-") && !arg.startsWith("/")) {
            configPath = arg;              // 兼容旧用法：直接传配置路径
        }
    }

#ifdef Q_OS_WIN
    // ---------- 命名互斥量防止多开 (仅限后台监控模式) ----------
    HANDLE hMutex = nullptr;
    if (!immediateShow) {
        // 获取当前所在桌面名称（default 或 Winlogon）
        HDESK hDesk = GetThreadDesktop(GetCurrentThreadId());
        wchar_t deskName[256] = L"Unknown";
        DWORD needed = 0;
        GetUserObjectInformationW(hDesk, UOI_NAME, deskName, 256, &needed);
        
        QString mutexName = QString("Local\\ScreenSaverBackgroundInstance_%1").arg(QString::fromWCharArray(deskName));

        // 同一桌面会话中只允许一个后台监控实例
        hMutex = CreateMutexW(nullptr, FALSE, (LPCWSTR)mutexName.utf16());
        if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
            qWarning() << "ScreenSaver background monitor is already running on this desktop, exiting.";
            CloseHandle(hMutex);
            return 0;
        }
    }
#endif

    // ---------- 加载配置 ----------
    ScreenSaverConfig config;

    // 如果相对路径，基于可执行文件目录查找
    if (QFileInfo(configPath).isRelative()) {
        QString appDir = QApplication::applicationDirPath();
        QString fullPath = appDir + "/" + configPath;
        if (QFileInfo::exists(fullPath)) {
            configPath = fullPath;
        }
    }

    if (!config.load(configPath)) {
        qWarning() << "Using default configuration";
    }

    // ---------- 加载本地字体（如果有） ----------
    QString fontFamily = config.fontFamily();
    QString appDirLocal = QApplication::applicationDirPath();
    QStringList fontFiles = { fontFamily, fontFamily + ".ttf", fontFamily + ".otf", fontFamily + ".ttc" };
    
    bool fontLoaded = false;
    // 1. 先尝试直接相对路径加载（支持配置里写死如 "fonts/myfont.ttf"）
    for (const QString &file : fontFiles) {
        QString fontPath = appDirLocal + "/" + file;
        if (QFileInfo::exists(fontPath)) {
            int fontId = QFontDatabase::addApplicationFont(fontPath);
            if (fontId != -1) {
                QStringList families = QFontDatabase::applicationFontFamilies(fontId);
                if (!families.isEmpty()) {
                    config.setFontFamily(families.at(0));
                    qDebug() << "ScreenSaver: Loaded local font directly:" << fontPath << "as family:" << families.at(0);
                    fontLoaded = true;
                    break;
                }
            }
        }
    }

    // 2. 如果没找到，递归扫描整个程序所在目录及所有子文件夹，寻找同名字体文件
    if (!fontLoaded && !fontFamily.isEmpty()) {
        QDirIterator it(appDirLocal, QStringList() << "*.ttf" << "*.otf" << "*.ttc", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext() && !fontLoaded) {
            it.next();
            QString fileName = it.fileName();
            // 忽略大小写匹配文件名
            bool match = false;
            for (const QString &expected : fontFiles) {
                if (fileName.compare(expected, Qt::CaseInsensitive) == 0) {
                    match = true;
                    break;
                }
            }
            
            if (match) {
                int fontId = QFontDatabase::addApplicationFont(it.filePath());
                if (fontId != -1) {
                    QStringList families = QFontDatabase::applicationFontFamilies(fontId);
                    if (!families.isEmpty()) {
                        config.setFontFamily(families.at(0));
                        qDebug() << "ScreenSaver: Loaded local font from subdirectory:" << it.filePath() << "as family:" << families.at(0);
                        fontLoaded = true;
                    }
                }
            }
        }
    }

    // ---------- 创建管理器 ----------
    ScreenSaverManager manager(config);

    // ---------- 创建空闲检测（系统级） ----------
    IdleMonitor idleMonitor;
    idleMonitor.setIdleTimeout(config.idleTimeoutSeconds());
    idleMonitor.setLoggedInIdleTimeout(config.loggedInIdleTimeoutSeconds());
    // 默认未登录状态，外部调用 idleMonitor.setLoggedIn(true) 切换
    idleMonitor.setLoggedIn(false);

    // 空闲超时 → 激活所有屏保
    QObject::connect(&idleMonitor, &IdleMonitor::triggered,
                     &manager, &ScreenSaverManager::activateAll);

    // 无论以何种模式启动，只要屏保被解散，都退回后台监控状态
    QObject::connect(&manager, &ScreenSaverManager::allDismissed,
                     &idleMonitor, &IdleMonitor::start);

    if (immediateShow) {
        // --show 模式：立即显示屏保（由服务启动时使用）
        qDebug() << "ScreenSaver: immediate show mode (launched by service)";
        manager.activateAll();
        
    } else {
        // 启动空闲检测
        idleMonitor.start();
    }

    qDebug() << "ScreenSaver application started";
    qDebug() << "  Config:" << configPath;
    qDebug() << "  Idle timeout (not logged in):" << config.idleTimeoutSeconds() << "seconds";
    qDebug() << "  Idle timeout (logged in):" << config.loggedInIdleTimeoutSeconds() << "seconds";
    qDebug() << "  Immediate show:" << immediateShow;

    return app.exec();
}
