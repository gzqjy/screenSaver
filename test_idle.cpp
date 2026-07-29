#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QThread>
#include <stdio.h>

qint64 getIdleTimeFromMutter() {
    QDBusInterface iface("org.gnome.Mutter.IdleMonitor", "/org/gnome/Mutter/IdleMonitor/Core", "org.gnome.Mutter.IdleMonitor", QDBusConnection::sessionBus());
    if (!iface.isValid()) return -1;
    QDBusReply<quint64> reply = iface.call("GetIdletime");
    if (reply.isValid()) return static_cast<qint64>(reply.value());
    return -1;
}

qint64 getIdleTimeFromProc() {
    FILE *fp = fopen("/proc/interrupts", "r");
    if (!fp) return -1;
    qint64 total = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "i8042") || strstr(line, "xhci") || strstr(line, "ehci") || strstr(line, "uhci") || strstr(line, "ohci")) {
            char *p = line;
            while (*p && (*p < '0' || *p > '9')) p++;
            while (*p) {
                if (*p >= '0' && *p <= '9') {
                    total += strtoll(p, &p, 10);
                } else if (*p == ' ' || *p == '\t') {
                    p++;
                } else {
                    break;
                }
            }
        }
    }
    fclose(fp);
    return total;
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    for (int i=0; i<5; i++) {
        qDebug() << "Mutter:" << getIdleTimeFromMutter() << " ProcInts:" << getIdleTimeFromProc();
        QThread::sleep(1);
    }
    return 0;
}
