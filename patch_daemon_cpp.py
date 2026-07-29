import sys

with open('/home/test/screenSaver/ScreenSaverDaemon.cpp', 'r') as f:
    lines = f.readlines()

out = []
skip = False
for line in lines:
    if line.startswith('bool ScreenSaverDaemon::isUserLoggedIn()'):
        skip = True
        
    if skip and line.startswith('// ===================================================================='):
        if '启动 ScreenSaver 进程' in ''.join(lines[lines.index(line)-2:lines.index(line)+2]):
            skip = False
            
    if skip:
        continue
        
    if line.startswith('bool ScreenSaverDaemon::findDisplaySession(DisplaySession &session)'):
        skip = True
        
    if line.startswith('        execl(exePath.c_str(), "ScreenSaver", "--show", nullptr);'):
        out.append('        execl(exePath.c_str(), "ScreenSaver", nullptr);\n')
        continue
        
    if line.startswith('void ScreenSaverDaemon::run(const std::string &screenSaverPath)'):
        out.append("""bool ScreenSaverDaemon::getActiveTTYDisplay(DisplaySession &session, std::string &activeTty)
{
    FILE *fp = fopen("/sys/class/tty/tty0/active", "r");
    if (!fp) return false;
    char ttyBuf[32] = {0};
    if (!fgets(ttyBuf, sizeof(ttyBuf), fp)) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    
    for (int i=0; ttyBuf[i]; i++) {
        if (ttyBuf[i] == '\\n' || ttyBuf[i] == '\\r') ttyBuf[i] = 0;
    }
    
    activeTty = ttyBuf;
    std::string vtStr = "vt";
    if (activeTty.find("tty") == 0) {
        vtStr += activeTty.substr(3);
    }
    
    std::string cmd = "ps -eo pid,cmd | grep -E 'Xorg|Xwayland'";
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return false;
    
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), p)) {
        std::string s(line);
        if (s.find("grep") != std::string::npos) continue;
        
        if (s.find(vtStr) == std::string::npos && s.find(activeTty) == std::string::npos) {
            continue;
        }
        
        std::istringstream iss(s);
        std::string token;
        std::string display;
        std::string auth;
        
        while (iss >> token) {
            if (token[0] == ':' && token.length() >= 2 && isdigit(token[1])) {
                display = token;
            } else if (token == "-auth") {
                iss >> auth;
            }
        }
        
        if (!display.empty()) {
            session.type = SessionType::X11;
            session.display = display;
            session.xauthority = auth;
            found = true;
            break;
        }
    }
    pclose(p);
    return found;
}

""")
        out.append(line)
        continue

    # Rewrite the run loop
    if 'pid_t childPid = -1;' in line:
        out.append(line)
        out.append('    std::string currentActiveTty = "";\n')
        continue

    if line.strip() == 'while (g_running) {':
        out.append(line)
        
        loop_code = """        sleep(CHECK_INTERVAL);
        if (!g_running) break;

        DisplaySession session;
        std::string activeTty;
        bool found = getActiveTTYDisplay(session, activeTty);
        
        if (found && activeTty != currentActiveTty) {
            logMsg("Active TTY changed from %s to %s", currentActiveTty.c_str(), activeTty.c_str());
            if (childPid > 0 && isProcessAlive(childPid)) {
                kill(childPid, SIGTERM);
                int status;
                waitpid(childPid, &status, 0);
            }
            childPid = -1;
            currentActiveTty = activeTty;
        }
        
        if (!found) {
            continue;
        }

        if (childPid > 0 && isProcessAlive(childPid)) {
            continue;
        }

        if (childPid > 0) {
            int status;
            waitpid(childPid, &status, WNOHANG);
            childPid = -1;
            logMsg("ScreenSaver dismissed by user on %s. Waiting 10 seconds...", activeTty.c_str());
            sleep(10);
            continue;
        }

        logMsg("Launching ScreenSaver on %s (hidden in background)...", activeTty.c_str());
        childPid = launchScreenSaver(screenSaverPath, session);
"""
        out.append(loop_code)
        skip = True
        continue
        
    if skip and line.strip() == 'if (childPid < 0)':
        skip = False
        
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverDaemon.cpp', 'w') as f:
    f.writelines(out)
