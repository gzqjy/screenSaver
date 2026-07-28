#ifndef IDLEMONITOR_H
#define IDLEMONITOR_H

#include <QObject>
#include <QTimer>

class IdleMonitor : public QObject
{
    Q_OBJECT
public:
    explicit IdleMonitor(QObject *parent = nullptr);

    /// 设置未登录状态空闲超时(秒)
    void setIdleTimeout(int seconds);
    /// 设置已登录状态空闲超时(秒)
    void setLoggedInIdleTimeout(int seconds);
    /// 设置当前登录状态
    void setLoggedIn(bool loggedIn);
    bool isLoggedIn() const { return m_loggedIn; }

    void start();
    void stop();

    /// 获取操作系统级别的空闲时间(毫秒)
    /// Windows: GetLastInputInfo
    /// Linux:   XScreenSaverQueryInfo (libXss)
    static quint64 getSystemIdleTimeMs();

signals:
    void triggered();

private slots:
    void checkIdle();

private:
    int currentTimeoutMs() const;

    QTimer *m_checkTimer;
    int     m_idleTimeoutMs;          // 未登录超时
    int     m_loggedInIdleTimeoutMs;  // 已登录超时
    bool    m_loggedIn;
    bool    m_active;
};

#endif // IDLEMONITOR_H
