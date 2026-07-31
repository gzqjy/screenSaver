#include "ScreenSaverConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

ScreenSaverConfig::ScreenSaverConfig()
{
    setDefaults();
}

void ScreenSaverConfig::setDefaults()
{
    m_imagePath          = "";
    m_opacity            = 0.8;
    m_text               = QString::fromUtf8("按任意键退出屏保");
    m_textPosition       = "center";
    m_textX              = -1;
    m_textY              = -1;
    m_textColor          = QColor(255, 255, 255);
    m_textSize           = 32;
    m_fontFamily         = "Microsoft YaHei";
    m_idleTimeoutSeconds = 300;
    m_loggedInIdleTimeoutSeconds = 600;
    m_slideIntervalSeconds = 10;
    m_backgroundColor    = QColor(0, 0, 0);
    m_backgroundOpacity  = 1.0;
}

bool ScreenSaverConfig::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ScreenSaverConfig: cannot open config file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "ScreenSaverConfig: JSON parse error:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "ScreenSaverConfig: root is not a JSON object";
        return false;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("image_path"))
        m_imagePath = obj["image_path"].toString();

    if (obj.contains("opacity"))
        m_opacity = qBound(0.0, obj["opacity"].toDouble(0.8), 1.0);

    if (obj.contains("text"))
        m_text = obj["text"].toString();

    if (obj.contains("text_position"))
        m_textPosition = obj["text_position"].toString();

    if (obj.contains("text_x"))
        m_textX = obj["text_x"].toInt(-1);

    if (obj.contains("text_y"))
        m_textY = obj["text_y"].toInt(-1);

    if (obj.contains("text_color"))
        m_textColor = QColor(obj["text_color"].toString());

    if (obj.contains("text_size"))
        m_textSize = qMax(8, obj["text_size"].toInt(32));

    if (obj.contains("font_family"))
        m_fontFamily = obj["font_family"].toString();

    if (obj.contains("idle_timeout_seconds"))
        m_idleTimeoutSeconds = qMax(1, obj["idle_timeout_seconds"].toInt(300));

    if (obj.contains("logged_in_idle_timeout_seconds"))
        m_loggedInIdleTimeoutSeconds = qMax(1, obj["logged_in_idle_timeout_seconds"].toInt(600));

    if (obj.contains("slide_interval_seconds"))
        m_slideIntervalSeconds = qMax(1, obj["slide_interval_seconds"].toInt(10));

    if (obj.contains("background_color"))
        m_backgroundColor = QColor(obj["background_color"].toString());

    if (obj.contains("background_opacity"))
        m_backgroundOpacity = qBound(0.0, obj["background_opacity"].toDouble(1.0), 1.0);

    qDebug() << "ScreenSaverConfig: loaded successfully from" << filePath;
    return true;
}
