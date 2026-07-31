#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

Window findLockScreen(Display *d, Window root, Window ignore) {
    Window root_ret, parent_ret, *children;
    unsigned int num_children;
    if (!XQueryTree(d, root, &root_ret, &parent_ret, &children, &num_children)) {
        return 0;
    }
    Window found = 0;
    for (int i = num_children - 1; i >= 0; --i) {
        Window child = children[i];
        if (child == ignore) continue;
        
        XClassHint hint;
        if (XGetClassHint(d, child, &hint)) {
            bool match = false;
            if (hint.res_name && hint.res_class) {
                if (strstr(hint.res_name, "screensaver") || (strstr(hint.res_name, "lock") && !strstr(hint.res_name, "clock")) || strstr(hint.res_name, "ksmserver") ||
                    strstr(hint.res_class, "screensaver") || (strstr(hint.res_class, "lock") && !strstr(hint.res_class, "clock")) || strstr(hint.res_class, "ksmserver")) {
                    match = true;
                }
            }
            if (hint.res_name) XFree(hint.res_name);
            if (hint.res_class) XFree(hint.res_class);
            
            if (match) {
                XWindowAttributes attr;
                if (XGetWindowAttributes(d, child, &attr)) {
                    if (attr.map_state == IsViewable) {
                        found = child;
                    }
                }
            }
        }
        
        if (!found) found = findLockScreen(d, child, ignore);
        if (found) break;
    }
    if (children) XFree(children);
    return found;
}

int main() {
    Display *d = XOpenDisplay(NULL);
    if (!d) return 1;
    Window lock = findLockScreen(d, DefaultRootWindow(d), 0);
    printf("Lock screen window: %lx\n", lock);
    XCloseDisplay(d);
    return 0;
}
