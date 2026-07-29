import sys

with open('/home/test/screenSaver/x11_helper.cpp', 'r') as f:
    content = f.read()

import re
new_func = '''
void reparentToLockScreen(unsigned long myWinId) {
    Display *d = XOpenDisplay(NULL);
    if (!d) return;
    Window lockWin = findLockScreen(d, DefaultRootWindow(d), myWinId);
    if (lockWin) {
        Window root, parent, *children;
        unsigned int num_children;
        if (XQueryTree(d, myWinId, &root, &parent, &children, &num_children)) {
            if (parent != lockWin) {
                XReparentWindow(d, myWinId, lockWin, 0, 0);
            }
            if (children) XFree(children);
        }
        XFlush(d);
    }
    XCloseDisplay(d);
}
'''

content = re.sub(r'void reparentToLockScreen\(unsigned long myWinId\) \{[\s\S]*?XCloseDisplay\(d\);\n\}', new_func.strip(), content)

with open('/home/test/screenSaver/x11_helper.cpp', 'w') as f:
    f.write(content)
