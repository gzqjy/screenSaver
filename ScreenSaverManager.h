#ifndef SCREENSAVERMANAGER_H
#define SCREENSAVERMANAGER_H

#include <QObject>
#include <QList>
#include <QTimer>
#include "ScreenSaverConfig.h"
#include "ScreenSaverWidget.h"

class ScreenSaverManager : public QObject
{
    Q_OBJECT
public:
    explicit ScreenSaverManager(const ScreenSaverConfig &config, QObject *parent = nullptr);
    ~ScreenSaverManager();

public slots:
    void activateAll();
    void deactivateAll();

signals:
    void allDismissed();

private slots:
    void onWidgetDismissed();
    void checkGlobalInput();

private:
    void killLockApp();

    const ScreenSaverConfig &m_config;
    QList<ScreenSaverWidget*> m_widgets;
    QTimer *m_inputCheckTimer;
    bool m_active;
    qint64 m_activationTime;
};

#endif // SCREENSAVERMANAGER_H
