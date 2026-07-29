import sys

with open('/home/test/screenSaver/ScreenSaverDaemon.h', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if 'static bool findDisplaySession(DisplaySession &session);' in line:
        out.append('    static bool getActiveTTYDisplay(DisplaySession &session, std::string &activeTty);\n')
        continue
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverDaemon.h', 'w') as f:
    f.writelines(out)
