import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    lines = f.readlines()

out = []
in_x11_block = False
x11_block = []

for line in lines:
    if line.strip() == '#ifdef Q_OS_LINUX' and '#include <X11/Xlib.h>' in ''.join(lines):
        # We need to extract the block we added at the top
        pass
    
    out.append(line)

# Actually, let's just do a clean replace using sed or python.
