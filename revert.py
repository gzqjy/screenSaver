import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    lines = f.readlines()

out = []
skip = False
for line in lines:
    if line.strip() == '#ifdef Q_OS_LINUX' and '#include <X11/Xlib.h>' in ''.join(lines):
        pass # this is just a condition check

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    content = f.read()

import re
# Remove the block at the top
content = re.sub(r'#ifdef Q_OS_LINUX\n#include <X11/Xlib.h>[\s\S]*?#endif\n', '', content, count=1)
# Remove the showEvent block
content = re.sub(r'#ifdef Q_OS_LINUX\n\s*Display \*d = XOpenDisplay\(nullptr\);[\s\S]*?#endif\n', '', content, count=1)
# Remove the hideEvent block
content = re.sub(r'#ifdef Q_OS_LINUX\n\s*Display \*d = XOpenDisplay\(nullptr\);[\s\S]*?#endif\n', '', content, count=1)

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'w') as f:
    f.write(content)
