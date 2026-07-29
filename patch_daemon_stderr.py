import sys

with open('/home/test/screenSaver/ScreenSaverDaemon.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if line.strip() == 'execl(exePath.c_str(), "ScreenSaver", nullptr);':
        out.append('        // Redirect stderr to log\n')
        out.append('        freopen("/tmp/screensaver_crash.log", "w", stderr);\n')
        out.append(line)
        continue
        
    if line.strip() == 'bool ScreenSaverDaemon::isProcessAlive(pid_t pid)':
        out.append(line)
        out.append('{\n')
        out.append('    if (pid <= 0) return false;\n')
        out.append('    int status;\n')
        out.append('    pid_t result = waitpid(pid, &status, WNOHANG);\n')
        out.append('    if (result == 0) return true;\n')
        out.append('    return false;\n')
        out.append('}\n')
        skip = True
        continue
        
    if 'skip' in locals() and skip:
        if line.strip() == '}':
            skip = False
        continue
        
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverDaemon.cpp', 'w') as f:
    f.writelines(out)
