#include "ScreenSaverWidget.h"
#ifdef Q_OS_LINUX
#include "x11_helper.h"
#endif
#ifdef Q_OS_LINUX
#include "x11_helper.h"
#endif
#include <QPainter>
#include <QKeyEvent>
#include <QDateTime>
#include <QMouseEvent>
#include <QScreen>
#include <QApplication>
#include <QDateTime>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ScreenSaverWidget::ScreenSaverWidget(const ScreenSaverConfig &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
    , m_mouseInitialized(false)
    , m_active(false)
{
    // 无边框 + 置顶 + X11 绕过窗口管理器 (用于在 Linux 锁屏界面上方绘制)
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::X11BypassWindowManagerHint);

    // 接收鼠标移动事件
    setMouseTracking(true);

    // 黑色背景
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_config.backgroundColor());
    setAutoFillBackground(true);
    setPalette(pal);

    // 图片轮播定时器
    m_slideTimer = new QTimer(this);
    m_slideTimer->setInterval(m_config.slideIntervalSeconds() * 1000);
    connect(m_slideTimer, &QTimer::timeout, this, &ScreenSaverWidget::onSlideTimer);

    m_reparentTimer = new QTimer(this);
    m_reparentTimer->setInterval(1000); // Check every second
    connect(m_reparentTimer, &QTimer::timeout, this, &ScreenSaverWidget::onReparentTimer);

    // 加载图片
    if (!m_config.imagePath().isEmpty()) {
        m_imageManager.loadFromPath(m_config.imagePath());
    }
}

ScreenSaverWidget::~ScreenSaverWidget()
{
}

void ScreenSaverWidget::activate()
{
    if (m_active) return;
    
    qDebug() << "ScreenSaverWidget: activating";
    m_active = true;
    m_mouseInitialized = false;
    m_activationTime = QDateTime::currentMSecsSinceEpoch();

    // 重新加载图片（配置可能已变更）
    if (!m_config.imagePath().isEmpty()) {
        m_imageManager.loadFromPath(m_config.imagePath());
    }

    // 获取主屏幕尺寸，全屏显示
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        setGeometry(screenGeometry);
    }

    // 隐藏鼠标光标
    setCursor(Qt::BlankCursor);

    // 全屏显示
    showFullScreen();

    // 确保获取焦点
    raise();
    activateWindow();
    setFocus();

    // 多图时启动轮播
    if (m_imageManager.imageCount() > 1) {
        m_slideTimer->start();
    }
    
    m_reparentTimer->start();
}

void ScreenSaverWidget::deactivate()
{
    if (!m_active) return;
    
    qDebug() << "ScreenSaverWidget: deactivating";
    m_active = false;
    m_slideTimer->stop();
    m_reparentTimer->stop();
    hide();
    emit dismissed();
}

// ---------- 绘制 ----------

void ScreenSaverWidget::hideEvent(QHideEvent *event)
{
#ifdef Q_OS_LINUX
    reparentToRoot(this->winId());
#endif
    QWidget::hideEvent(event);
}

void ScreenSaverWidget::showEvent(QShowEvent *event)
{
#ifdef Q_OS_LINUX
    reparentToLockScreen(this->winId());
#endif
    QWidget::showEvent(event);

#ifdef Q_OS_WIN
    HWND hwnd = (HWND)winId();
    // 获取当前处于前台的窗口及其线程ID
    HWND hCurWnd = ::GetForegroundWindow();
    DWORD dwMyID = ::GetCurrentThreadId();
    DWORD dwCurID = ::GetWindowThreadProcessId(hCurWnd, nullptr);
    
    // 强制附加线程输入，绕过 Windows 的防止后台抢占焦点的限制 (Foreground Lock)
    if (dwCurID != 0 && dwCurID != dwMyID) {
        ::AttachThreadInput(dwCurID, dwMyID, TRUE);
    }
    
    // 黑科技：尝试使用未公开 API 强行突破 DWM Z-Band 限制（针对 Winlogon 桌面）
    // 只有在 Winlogon (锁屏) 桌面才需要 SetWindowBand 强行盖住系统 UI
    // 在 default 桌面（正常已登录），调用这个函数会导致 Windows 强行裁剪副屏的画面！
    bool isLockScreen = false;
    HDESK hCurrentDesk = GetThreadDesktop(GetCurrentThreadId());
    wchar_t deskName[256] = {0};
    DWORD needed = 0;
    if (GetUserObjectInformationW(hCurrentDesk, UOI_NAME, deskName, sizeof(deskName), &needed)) {
        if (_wcsicmp(deskName, L"Winlogon") == 0) {
            isLockScreen = true;
        }
    }

    if (isLockScreen) {
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            typedef BOOL(WINAPI *SetWindowBand_t)(HWND, HWND, DWORD);
            SetWindowBand_t pSetWindowBand = (SetWindowBand_t)GetProcAddress(hUser32, "SetWindowBand");
            if (pSetWindowBand) {
                pSetWindowBand(hwnd, nullptr, 18); // ZBID_ABOVELOCK_UX
                pSetWindowBand(hwnd, nullptr, 2);  // ZBID_UIACCESS
            }
        }
    }

    // 强制置顶并激活（显式传递坐标和大小，防止 Windows 忽略 Qt 的设置）
    QRect geo = geometry();
    ::SetWindowPos(hwnd, HWND_TOPMOST, geo.x(), geo.y(), geo.width(), geo.height(), SWP_SHOWWINDOW);
    ::SetForegroundWindow(hwnd);
    ::SetFocus(hwnd);
    ::SetActiveWindow(hwnd);
    
    // 取消附加
    if (dwCurID != 0 && dwCurID != dwMyID) {
        ::AttachThreadInput(dwCurID, dwMyID, FALSE);
    }
#endif
}

void ScreenSaverWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    drawBackground(painter);
    drawImage(painter);
    drawText(painter);
}

void ScreenSaverWidget::drawBackground(QPainter &painter)
{
    painter.fillRect(rect(), m_config.backgroundColor());
}

void ScreenSaverWidget::drawImage(QPainter &painter)
{
    if (!m_imageManager.hasImages())
        return;

    QPixmap scaled = m_imageManager.scaledImage(size());
    if (scaled.isNull())
        return;

    // 居中绘制
    int x = (width()  - scaled.width())  / 2;
    int y = (height() - scaled.height()) / 2;

    // 设置透明度
    painter.setOpacity(m_config.opacity());
    painter.drawPixmap(x, y, scaled);
    painter.setOpacity(1.0);
}

void ScreenSaverWidget::drawText(QPainter &painter)
{
    QString text = m_config.text();
    if (text.isEmpty())
        return;

    // 设置字体
    QFont font(m_config.fontFamily(), m_config.textSize());
    painter.setFont(font);

    // 计算文字尺寸
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(QRect(0, 0, width(), height()),
                                     Qt::AlignCenter | Qt::TextWordWrap, text);
    QPoint pos = calcTextPosition(size(), textRect.size());

    // 绘制文字阴影（增强可读性）
    painter.setPen(QColor(0, 0, 0, 160));
    painter.drawText(pos.x() + 2, pos.y() + 2,
                     textRect.width(), textRect.height(),
                     Qt::AlignCenter | Qt::TextWordWrap, text);

    // 绘制文字
    painter.setPen(m_config.textColor());
    painter.drawText(pos.x(), pos.y(),
                     textRect.width(), textRect.height(),
                     Qt::AlignCenter | Qt::TextWordWrap, text);
}

QPoint ScreenSaverWidget::calcTextPosition(const QSize &widgetSize, const QSize &textSize) const
{
    // 如果指定了自定义坐标
    if (m_config.textX() >= 0 && m_config.textY() >= 0) {
        return QPoint(m_config.textX(), m_config.textY());
    }

    int margin = 40;
    QString pos = m_config.textPosition().toLower();

    if (pos == "top-left") {
        return QPoint(margin, margin);
    } else if (pos == "top-right") {
        return QPoint(widgetSize.width() - textSize.width() - margin, margin);
    } else if (pos == "bottom-left") {
        return QPoint(margin, widgetSize.height() - textSize.height() - margin);
    } else if (pos == "bottom-right") {
        return QPoint(widgetSize.width() - textSize.width() - margin,
                      widgetSize.height() - textSize.height() - margin);
    } else {
        // center (默认)
        return QPoint((widgetSize.width()  - textSize.width())  / 2,
                      (widgetSize.height() - textSize.height()) / 2);
    }
}

// ---------- 输入事件 → 退出屏保 ----------

void ScreenSaverWidget::keyPressEvent(QKeyEvent * /*event*/)
{
    if (QDateTime::currentMSecsSinceEpoch() - m_activationTime < 500) return;
    deactivate();
}

void ScreenSaverWidget::mousePressEvent(QMouseEvent * /*event*/)
{
    if (QDateTime::currentMSecsSinceEpoch() - m_activationTime < 500) return;
    deactivate();
}

void ScreenSaverWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (QDateTime::currentMSecsSinceEpoch() - m_activationTime < 500) {
        m_lastMousePos = event->pos();
        m_mouseInitialized = true;
        return;
    }

    // 首次移动记录位置，之后判断移动距离
    if (!m_mouseInitialized) {
        m_lastMousePos = event->pos();
        m_mouseInitialized = true;
        return;
    }

    QPoint delta = event->pos() - m_lastMousePos;
    // 移动超过5像素才退出（避免微小抖动误触）
    if (delta.manhattanLength() > 5) {
        deactivate();
    }
}

// ---------- 轮播 ----------

void ScreenSaverWidget::onSlideTimer()
{
    m_imageManager.nextImage();
    update(); // 触发重绘
}

void ScreenSaverWidget::onReparentTimer()
{
#ifdef Q_OS_LINUX
    reparentToLockScreen(this->winId());
#endif
}
