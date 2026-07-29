import sys

with open('/home/test/screenSaver/IdleMonitor.h', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if 'static quint64 getSystemIdleTimeMs();' in line:
        lines.insert(i+1, '    static void reportActivity();\n')
        break

with open('/home/test/screenSaver/IdleMonitor.h', 'w') as f:
    f.writelines(lines)

with open('/home/test/screenSaver/IdleMonitor.cpp', 'r') as f:
    lines = f.readlines()

out = []
for i, line in enumerate(lines):
    if line.startswith('quint64 IdleMonitor::getSystemIdleTimeMs()'):
        out.append('static qint64 s_manualActivityTime = 0;\n\n')
        out.append('void IdleMonitor::reportActivity()\n{\n    s_manualActivityTime = QDateTime::currentMSecsSinceEpoch();\n}\n\n')
        out.append('static quint64 getSystemIdleTimeMsInternal()\n')
    else:
        out.append(line)

out.append('\nquint64 IdleMonitor::getSystemIdleTimeMs()\n')
out.append('{\n')
out.append('    quint64 sysIdle = getSystemIdleTimeMsInternal();\n')
out.append('    qint64 manualIdle = QDateTime::currentMSecsSinceEpoch() - s_manualActivityTime;\n')
out.append('    if (manualIdle >= 0 && static_cast<quint64>(manualIdle) < sysIdle) {\n')
out.append('        return static_cast<quint64>(manualIdle);\n')
out.append('    }\n')
out.append('    return sysIdle;\n')
out.append('}\n')

with open('/home/test/screenSaver/IdleMonitor.cpp', 'w') as f:
    f.writelines(out)

with open('/home/test/screenSaver/ScreenSaverManager.cpp', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if line.startswith('void ScreenSaverManager::deactivateAll()'):
        lines.insert(i+2, '    IdleMonitor::reportActivity();\n')
        break

has_idle_include = any('IdleMonitor.h' in line for line in lines)
if not has_idle_include:
    for i, line in enumerate(lines):
        if line.startswith('#include'):
            lines.insert(i, '#include "IdleMonitor.h"\n')
            break

with open('/home/test/screenSaver/ScreenSaverManager.cpp', 'w') as f:
    f.writelines(lines)

