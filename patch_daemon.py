import sys

with open('/home/test/screenSaver/ScreenSaverDaemon.h', 'r') as f:
    lines = f.readlines()

out = []
skip = False
for line in lines:
    if line.strip() == 'bool findDisplaySession(DisplaySession &session);':
        out.append('    bool getActiveTTYDisplay(DisplaySession &session, std::string &activeTty);\n')
        continue
    if line.strip() == 'bool getActiveLoginctlSession(std::string &display, SessionType &type);':
        continue
    if line.strip() == 'bool isUserLoggedIn();':
        continue
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverDaemon.h', 'w') as f:
    f.writelines(out)

