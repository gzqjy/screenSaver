import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if line.strip() == '#include "ScreenSaverWidget.h"':
        out.append(line)
        out.append('#ifdef Q_OS_LINUX\n')
        out.append('#include <X11/Xlib.h>\n')
        out.append('#include <X11/Xutil.h>\n')
        out.append('static Window findLockScreen(Display *d, Window root, Window ignore) {\n')
        out.append('    Window root_ret, parent_ret, *children;\n')
        out.append('    unsigned int num_children;\n')
        out.append('    if (!XQueryTree(d, root, &root_ret, &parent_ret, &children, &num_children)) return 0;\n')
        out.append('    Window found = 0;\n')
        out.append('    for (int i = num_children - 1; i >= 0; --i) {\n')
        out.append('        Window child = children[i];\n')
        out.append('        if (child == ignore) continue;\n')
        out.append('        XClassHint hint;\n')
        out.append('        if (XGetClassHint(d, child, &hint)) {\n')
        out.append('            if (hint.res_name && hint.res_class) {\n')
        out.append('                if (strstr(hint.res_name, "screensaver") || strstr(hint.res_name, "lock") || strstr(hint.res_name, "ksmserver") ||\n')
        out.append('                    strstr(hint.res_class, "screensaver") || strstr(hint.res_class, "lock") || strstr(hint.res_class, "ksmserver")) {\n')
        out.append('                    found = child;\n')
        out.append('                }\n')
        out.append('            }\n')
        out.append('            if (hint.res_name) XFree(hint.res_name);\n')
        out.append('            if (hint.res_class) XFree(hint.res_class);\n')
        out.append('        }\n')
        out.append('        if (found) break;\n')
        out.append('        found = findLockScreen(d, child, ignore);\n')
        out.append('        if (found) break;\n')
        out.append('    }\n')
        out.append('    if (children) XFree(children);\n')
        out.append('    return found;\n')
        out.append('}\n')
        out.append('#endif\n')
        continue

    if line.strip() == 'void ScreenSaverWidget::showEvent(QShowEvent *event)':
        out.append(line)
        out.append('{\n')
        out.append('#ifdef Q_OS_LINUX\n')
        out.append('    Display *d = XOpenDisplay(nullptr);\n')
        out.append('    if (d) {\n')
        out.append('        Window myWin = this->winId();\n')
        out.append('        Window lockWin = findLockScreen(d, DefaultRootWindow(d), myWin);\n')
        out.append('        if (lockWin) {\n')
        out.append('            qDebug() << "Reparenting ScreenSaver to lock screen:" << lockWin;\n')
        out.append('            XReparentWindow(d, myWin, lockWin, 0, 0);\n')
        out.append('        }\n')
        out.append('        XFlush(d);\n')
        out.append('        XCloseDisplay(d);\n')
        out.append('    }\n')
        out.append('#endif\n')
        skip = True
        continue
        
    if 'skip' in locals() and skip:
        if line.strip() == '{':
            skip = False
        continue

    if line.strip() == 'void ScreenSaverWidget::hideEvent(QHideEvent *event)':
        out.append(line)
        out.append('{\n')
        out.append('#ifdef Q_OS_LINUX\n')
        out.append('    Display *d = XOpenDisplay(nullptr);\n')
        out.append('    if (d) {\n')
        out.append('        Window myWin = this->winId();\n')
        out.append('        XReparentWindow(d, myWin, DefaultRootWindow(d), 0, 0);\n')
        out.append('        XFlush(d);\n')
        out.append('        XCloseDisplay(d);\n')
        out.append('    }\n')
        out.append('#endif\n')
        skip2 = True
        continue
        
    if 'skip2' in locals() and skip2:
        if line.strip() == '{':
            skip2 = False
        continue

    out.append(line)

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'w') as f:
    f.writelines(out)
