import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if line.strip() == '#include "ScreenSaverWidget.h"':
        out.append(line)
        out.append('#ifdef Q_OS_LINUX\n')
        out.append('#include "x11_helper.h"\n')
        out.append('#endif\n')
        continue

    if line.strip() == 'void ScreenSaverWidget::showEvent(QShowEvent *event)':
        out.append(line)
        out.append('{\n')
        out.append('#ifdef Q_OS_LINUX\n')
        out.append('    reparentToLockScreen(this->winId());\n')
        out.append('#endif\n')
        out.append('    QWidget::showEvent(event);\n')
        out.append('}\n\n')
        out.append('void ScreenSaverWidget::hideEvent(QHideEvent *event)\n')
        out.append('{\n')
        out.append('#ifdef Q_OS_LINUX\n')
        out.append('    reparentToRoot(this->winId());\n')
        out.append('#endif\n')
        out.append('    QWidget::hideEvent(event);\n')
        out.append('}\n')
        skip = True
        continue
        
    if 'skip' in locals() and skip:
        if line.strip() == '{':
            skip = False
        continue

    out.append(line)

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'w') as f:
    f.writelines(out)
