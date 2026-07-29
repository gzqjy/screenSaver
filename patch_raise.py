import sys

with open('/home/test/screenSaver/ScreenSaverWidget.h', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if 'QTimer *m_imageTimer;' in line:
        out.append(line)
        out.append('    QTimer *m_raiseTimer;\n')
        continue
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverWidget.h', 'w') as f:
    f.writelines(out)

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if 'm_imageTimer = new QTimer(this);' in line:
        out.append(line)
        out.append('    m_raiseTimer = new QTimer(this);\n')
        out.append('    connect(m_raiseTimer, &QTimer::timeout, this, [this]() {\n')
        out.append('        this->raise();\n')
        out.append('        this->activateWindow();\n')
        out.append('    });\n')
        continue
    if 'm_imageTimer->start(config.slideIntervalSeconds() * 1000);' in line:
        out.append(line)
        out.append('    m_raiseTimer->start(500);\n')
        continue
    if 'm_imageTimer->stop();' in line:
        out.append(line)
        out.append('    m_raiseTimer->stop();\n')
        continue
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'w') as f:
    f.writelines(out)
