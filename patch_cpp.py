import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    content = f.read()

import re

# In constructor
content = re.sub(r'm_slideTimer = new QTimer\(this\);', 
                 'm_slideTimer = new QTimer(this);\n    m_reparentTimer = new QTimer(this);\n    m_reparentTimer->setInterval(1000);\n    connect(m_reparentTimer, &QTimer::timeout, this, &ScreenSaverWidget::onReparentTimer);',
                 content)

# In activate()
content = re.sub(r'm_slideTimer->start\(\);', 'm_slideTimer->start();\n    }\n    m_reparentTimer->start();\n    // removed trailing } because we inserted one before it? No, wait.', content)
# Let's do it safer.

