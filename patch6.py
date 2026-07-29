import sys

with open('/home/test/screenSaver/IdleMonitor.cpp', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if 'strstr(line, "i8042")' in line and 'xhci' in line:
        lines[i] = '        if (strstr(line, "i8042") || strstr(line, "xhci") || strstr(line, "ehci") || strstr(line, "uhci") || strstr(line, "ohci")) {\n'
        break

with open('/home/test/screenSaver/IdleMonitor.cpp', 'w') as f:
    f.writelines(lines)
