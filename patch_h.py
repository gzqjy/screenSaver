import sys
with open('/home/test/screenSaver/ScreenSaverWidget.h', 'r') as f:
    content = f.read()

import re
content = re.sub(r'QTimer\s*\*m_slideTimer;', 'QTimer       *m_slideTimer;\n    QTimer       *m_reparentTimer;', content)
content = re.sub(r'void onSlideTimer\(\);', 'void onSlideTimer();\n    void onReparentTimer();', content)

with open('/home/test/screenSaver/ScreenSaverWidget.h', 'w') as f:
    f.write(content)
