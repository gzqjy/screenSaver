#ifdef __linux__
#include "x11_helper.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>

static Window findLockScreen(Display *d, Window root, Window ignore) {
    Window root_ret, parent_ret, *children;
    unsigned int num_children;
    if (!XQueryTree(d, root, &root_ret, &parent_ret, &children, &num_children)) return 0;
    
    Window found = 0;
    for (int i = num_children - 1; i >= 0; --i) {
        Window child = children[i];
        if (child == ignore) continue;
        
        XClassHint hint;
        if (XGetClassHint(d, child, &hint)) {
            if (hint.res_name && hint.res_class) {
                if (strstr(hint.res_name, "screensaver") || strstr(hint.res_name, "lock") || strstr(hint.res_name, "ksmserver") ||
                    strstr(hint.res_class, "screensaver") || strstr(hint.res_class, "lock") || strstr(hint.res_class, "ksmserver")) {
                    found = child;
                }
            }
            if (hint.res_name) XFree(hint.res_name);
            if (hint.res_class) XFree(hint.res_class);
        }
        
        if (found) break;
        found = findLockScreen(d, child, ignore);
        if (found) break;
    }
    if (children) XFree(children);
    return found;
}

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

void reparentToRoot(unsigned long myWinId) {
    Display *d = XOpenDisplay(NULL);
    if (!d) return;
    XReparentWindow(d, myWinId, DefaultRootWindow(d), 0, 0);
    XFlush(d);
    XCloseDisplay(d);
}
#endif
