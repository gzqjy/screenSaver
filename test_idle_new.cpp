#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include "IdleMonitor.h"
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTimer t;
    QObject::connect(&t, &QTimer::timeout, [](){
        qDebug() << "SysIdle:" << IdleMonitor::getSystemIdleTimeMs();
    });
    t.start(1000);
    return app.exec();
}
