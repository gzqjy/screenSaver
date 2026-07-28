#ifndef SCREENSAVERCONFIG_H
#define SCREENSAVERCONFIG_H

#include <QString>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>

class ScreenSaverConfig
{
public:
    ScreenSaverConfig();

    bool load(const QString &filePath);

    // 图片设置
    QString imagePath() const { return m_imagePath; }
    qreal opacity() const { return m_opacity; }

    // 文字设置
    QString text() const { return m_text; }
    QString textPosition() const { return m_textPosition; }
    int textX() const { return m_textX; }
    int textY() const { return m_textY; }
    QColor textColor() const { return m_textColor; }
    int textSize() const { return m_textSize; }
    QString fontFamily() const { return m_fontFamily; }

    // 超时设置
    int idleTimeoutSeconds() const { return m_idleTimeoutSeconds; }
    int loggedInIdleTimeoutSeconds() const { return m_loggedInIdleTimeoutSeconds; }
    int slideIntervalSeconds() const { return m_slideIntervalSeconds; }

    // 背景
    QColor backgroundColor() const { return m_backgroundColor; }

private:
    void setDefaults();

    QString m_imagePath;
    qreal   m_opacity;
    QString m_text;
    QString m_textPosition;
    int     m_textX;
    int     m_textY;
    QColor  m_textColor;
    int     m_textSize;
    QString m_fontFamily;
    int     m_idleTimeoutSeconds;
    int     m_loggedInIdleTimeoutSeconds;
    int     m_slideIntervalSeconds;
    QColor  m_backgroundColor;
};

#endif // SCREENSAVERCONFIG_H
