#include <cstdio>
#include <cstring>
#include <iostream>

bool isUserLoggedIn() {
    FILE *fp = popen("loginctl list-sessions --no-legend 2>/dev/null", "r");
    if (!fp) return false;

    char line[512];
    bool activeUserSession = false;
    while (fgets(line, sizeof(line), fp)) {
        unsigned int uid = 0;
        char session_id[128] = {};
        char user[128] = {};
        if (sscanf(line, "%127s %u %127s", session_id, &uid, user) >= 2) {
            if (uid >= 1000) {
                char cmd[256];
                snprintf(cmd, sizeof(cmd), "loginctl show-session %s -p LockedHint --value 2>/dev/null", session_id);
                FILE *pLock = popen(cmd, "r");
                bool isLocked = false;
                if (pLock) {
                    char lockBuf[64];
                    if (fgets(lockBuf, sizeof(lockBuf), pLock)) {
                        if (strncmp(lockBuf, "yes", 3) == 0) {
                            isLocked = true;
                        }
                    }
                    pclose(pLock);
                }
                
                if (!isLocked) {
                    activeUserSession = true;
                    break;
                }
            }
        }
    }
    pclose(fp);
    return activeUserSession;
}

int main() {
    std::cout << "isUserLoggedIn: " << isUserLoggedIn() << std::endl;
    return 0;
}
