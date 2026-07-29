#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>

int main() {
    FILE *fp = fopen("/sys/class/tty/tty0/active", "r");
    if (!fp) return 1;
    char activeTty[32] = {0};
    fgets(activeTty, sizeof(activeTty), fp);
    fclose(fp);
    
    for (int i=0; activeTty[i]; i++) {
        if (activeTty[i] == '\n' || activeTty[i] == '\r') activeTty[i] = 0;
    }
    
    std::string ttyStr(activeTty);
    std::string vtStr = "vt";
    if (ttyStr.find("tty") == 0) {
        vtStr += ttyStr.substr(3);
    }
    
    std::cout << "Active TTY: " << ttyStr << " / " << vtStr << std::endl;
    
    std::string cmd = "ps -eo pid,cmd | grep -E 'Xorg|Xwayland'";
    FILE *p = popen(cmd.c_str(), "r");
    char line[1024];
    while (fgets(line, sizeof(line), p)) {
        std::string s(line);
        if (s.find("grep") != std::string::npos) continue;
        
        std::cout << "Found X: " << s;
        if (s.find(vtStr) != std::string::npos || s.find(ttyStr) != std::string::npos) {
            std::cout << "  -> MATCHES ACTIVE TTY!" << std::endl;
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
            std::cout << "  -> DISPLAY=" << display << " AUTH=" << auth << std::endl;
        }
    }
    pclose(p);
    return 0;
}
