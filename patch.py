import sys

with open('/home/test/screenSaver/ScreenSaverDaemon.cpp', 'r') as f:
    lines = f.readlines()

out = []
in_x11 = False
in_find = False

for i, line in enumerate(lines):
    if line.startswith('bool ScreenSaverDaemon::findX11Session(DisplaySession &session)'):
        in_x11 = True
        out.append(line)
        out.append('{\n    session.type = SessionType::X11;\n    if (session.display.empty()) session.display = ":0";\n\n')
        out.append('    DIR *dir = opendir("/proc");\n    if (dir) {\n        struct dirent *entry;\n        while ((entry = readdir(dir)) != nullptr) {\n            if (entry->d_type != DT_DIR) continue;\n            int pid = atoi(entry->d_name);\n            if (pid <= 0) continue;\n            \n            char commPath[64];\n            snprintf(commPath, sizeof(commPath), "/proc/%d/comm", pid);\n            std::ifstream ifs(commPath);\n            std::string comm;\n            if (ifs.is_open() && std::getline(ifs, comm)) {\n                while (!comm.empty() && (comm.back() == \'\\n\' || comm.back() == \'\\r\')) comm.pop_back();\n                if (comm == "Xorg" || comm == "X") {\n                    char cmdPath[64];\n                    snprintf(cmdPath, sizeof(cmdPath), "/proc/%d/cmdline", pid);\n                    std::ifstream ifsCmd(cmdPath, std::ios::binary);\n                    if (ifsCmd.is_open()) {\n                        std::string content((std::istreambuf_iterator<char>(ifsCmd)), std::istreambuf_iterator<char>());\n                        bool match = false;\n                        size_t pos = 0;\n                        while (pos < content.size()) {\n                            size_t end = content.find(\'\\0\', pos);\n                            if (end == std::string::npos) end = content.size();\n                            std::string arg = content.substr(pos, end - pos);\n                            if (arg == session.display) {\n                                match = true;\n                                break;\n                            }\n                            pos = end + 1;\n                        }\n                        if (match || content.find(session.display) != std::string::npos) {\n                            logMsg("X11: found active X server PID: %d for display %s", pid, session.display.c_str());\n                            std::string auth = getAuthFromXorgCmdline(pid);\n                            if (!auth.empty() && fileReadable(auth)) {\n                                session.xauthority = auth;\n                            } else {\n                                std::string envXauth = readEnvFromProc(pid, "XAUTHORITY");\n                                if (!envXauth.empty() && fileReadable(envXauth)) session.xauthority = envXauth;\n                            }\n                            break;\n                        }\n                    }\n                }\n            }\n        }\n        closedir(dir);\n    }\n\n    if (session.xauthority.empty()) {\n        session.xauthority = findXauthorityFile(session.display);\n    }\n\n    return !session.display.empty();\n}\n')
        continue
    if in_x11:
        if line.startswith('// ===================================================================='):
            in_x11 = False
            out.append(line)
        continue

    if line.startswith('bool ScreenSaverDaemon::findDisplaySession(DisplaySession &session)'):
        in_find = True
        out.append(line)
        out.append('{\n    session.type = SessionType::Unknown;\n    session.display.clear();\n    session.xauthority.clear();\n    session.waylandDisplay.clear();\n    session.xdgRuntimeDir.clear();\n\n    std::string activeDisplay;\n    SessionType activeType;\n    if (getActiveLoginctlSession(activeDisplay, activeType)) {\n        if (activeType == SessionType::X11) {\n            logMsg("Active session is X11, Display: %s", activeDisplay.c_str());\n            session.display = activeDisplay;\n            return findX11Session(session);\n        } else if (activeType == SessionType::Wayland) {\n            logMsg("Active session is Wayland, Display: %s", activeDisplay.c_str());\n            session.waylandDisplay = activeDisplay;\n            return findWaylandSession(session);\n        }\n    }\n\n    logMsg("no active graphical session found via loginctl, trying fallback...");\n    if (findWaylandSession(session)) return true;\n    return findX11Session(session);\n}\n')
        continue
    if in_find:
        if line.startswith('// ===================================================================='):
            in_find = False
            out.append(line)
        continue

    out.append(line)
    if line.strip() == "return -1;" and lines[i+1].strip() == "}":
        out.append("}\n\nstd::string ScreenSaverDaemon::getAuthFromXorgCmdline(pid_t pid)\n{\n    char path[64];\n    snprintf(path, sizeof(path), \"/proc/%d/cmdline\", pid);\n    std::ifstream ifs(path, std::ios::binary);\n    if (!ifs.is_open()) return \"\";\n\n    std::string content((std::istreambuf_iterator<char>(ifs)),\n                         std::istreambuf_iterator<char>());\n    \n    std::vector<std::string> args;\n    size_t pos = 0;\n    while (pos < content.size()) {\n        size_t end = content.find('\\0', pos);\n        if (end == std::string::npos) break;\n        args.push_back(content.substr(pos, end - pos));\n        pos = end + 1;\n    }\n\n    for (size_t i = 0; i < args.size(); ++i) {\n        if (args[i] == \"-auth\" && i + 1 < args.size()) {\n            return args[i+1];\n        }\n    }\n    return \"\";\n}\n\nbool ScreenSaverDaemon::getActiveLoginctlSession(std::string &display, SessionType &type)\n{\n    FILE *fp = popen(\"loginctl list-sessions --no-legend 2>/dev/null\", \"r\");\n    if (!fp) return false;\n\n    char line[512];\n    std::string activeDisplay;\n    SessionType activeType = SessionType::Unknown;\n\n    while (fgets(line, sizeof(line), fp)) {\n        char session_id[128] = {};\n        if (sscanf(line, \"%127s\", session_id) >= 1) {\n            char cmd[256];\n            snprintf(cmd, sizeof(cmd), \"loginctl show-session %s -p State -p Type -p Display --value 2>/dev/null\", session_id);\n            FILE *pProp = popen(cmd, \"r\");\n            if (pProp) {\n                char buf[128];\n                bool isStateActive = false;\n                std::string sType, sDisplay;\n                while (fgets(buf, sizeof(buf), pProp)) {\n                    std::string val = buf;\n                    while (!val.empty() && (val.back() == '\\n' || val.back() == '\\r')) val.pop_back();\n                    if (val == \"active\") isStateActive = true;\n                    else if (val == \"x11\") sType = \"x11\";\n                    else if (val == \"wayland\") sType = \"wayland\";\n                    else if (val.find(\":\") == 0) sDisplay = val;\n                }\n                pclose(pProp);\n\n                if (isStateActive && !sType.empty() && !sDisplay.empty()) {\n                    activeDisplay = sDisplay;\n                    if (sType == \"x11\") activeType = SessionType::X11;\n                    else if (sType == \"wayland\") activeType = SessionType::Wayland;\n                    break;\n                }\n            }\n        }\n    }\n    pclose(fp);\n\n    if (activeType != SessionType::Unknown) {\n        display = activeDisplay;\n        type = activeType;\n        return true;\n    }\n    return false;\n}\n")
        # skip the next line which is '}'
        lines[i+1] = ''

with open('/home/test/screenSaver/ScreenSaverDaemon.cpp', 'w') as f:
    f.writelines(out)
