import sys

with open('/home/test/screenSaver/IdleMonitor.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if line.strip() == 'if (idle >= 0) return static_cast<quint64>(idle);':
        if 'Mutter' in ''.join(out[-2:]):
            out.append('    if (idle >= 0) { /*qDebug() << "Mutter sysIdle:" << idle;*/ return static_cast<quint64>(idle); }\n')
            continue
        elif 'KDE' in ''.join(out[-2:]):
            out.append('    if (idle >= 0) { /*qDebug() << "KDE sysIdle:" << idle;*/ return static_cast<quint64>(idle); }\n')
            continue
        elif 'freedesktop' in ''.join(out[-2:]):
            out.append('    if (idle >= 0) { /*qDebug() << "freedesktop sysIdle:" << idle;*/ return static_cast<quint64>(idle); }\n')
            continue
        elif 'X11' in ''.join(out[-2:]):
            out.append('    if (idle >= 0) { /*qDebug() << "X11 sysIdle:" << idle;*/ return static_cast<quint64>(idle); }\n')
            continue
        elif 'ProcInterrupts' in ''.join(out[-2:]):
            out.append('    if (idle >= 0) { /*qDebug() << "Proc sysIdle:" << idle;*/ return static_cast<quint64>(idle); }\n')
            continue
    out.append(line)

with open('/home/test/screenSaver/IdleMonitor.cpp', 'w') as f:
    f.writelines(out)
