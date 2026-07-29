import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    content = f.read()

import re
# Find where showEvent was supposed to be (around line 110)
content = re.sub(r'\}\n\n\n#ifdef Q_OS_WIN', 
                 '}\n\nvoid ScreenSaverWidget::showEvent(QShowEvent *event)\n{\n#ifdef Q_OS_LINUX\n    reparentToLockScreen(this->winId());\n#endif\n    QWidget::showEvent(event);\n\n#ifdef Q_OS_WIN',
                 content)

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'w') as f:
    f.write(content)
