#ifndef SCREENSAVERWIDGET_H
#define SCREENSAVERWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QPoint>
#include <QDateTime>
#include "ScreenSaverConfig.h"
#include "ImageManager.h"

class ScreenSaverWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenSaverWidget(const ScreenSaverConfig &config, QWidget *parent = nullptr);
    ~ScreenSaverWidget();

    void activate();
    void deactivate();
    bool isActive() const { return m_active; }

signals:
    void dismissed();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onSlideTimer();
    void onReparentTimer();

private:
    void drawBackground(QPainter &painter);
    void drawImage(QPainter &painter);
    void drawText(QPainter &painter);
    QPoint calcTextPosition(const QSize &widgetSize, const QSize &textSize) const;

    const ScreenSaverConfig &m_config;
    ImageManager  m_imageManager;
    QTimer       *m_slideTimer;
    QTimer       *m_reparentTimer;
    QPoint        m_lastMousePos;
    bool          m_mouseInitialized;
    bool          m_active;
    qint64        m_activationTime;
};

#endif // SCREENSAVERWIDGET_H
