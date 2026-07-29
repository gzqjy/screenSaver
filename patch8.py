import sys

with open('/home/test/screenSaver/IdleMonitor.cpp', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if line.startswith('#include <QCursor>'):
        lines.insert(i, '#include <QLibrary>\n')
        break

# Find the block where we check QCursor::pos()
for i, line in enumerate(lines):
    if line.strip() == 'idleMs = getSystemIdleTimeMs(); // 重新获取':
        insert_idx = i + 2
        
        insert_code = """
#ifdef Q_OS_LINUX
    // Linux X11 终极后备：通过轮询全局键盘状态检测键盘活动 (动态加载以兼容无 X11 的环境)
    static QLibrary x11lib("X11");
    if (x11lib.load()) {
        typedef void* (*XOpenDisplayFunc)(const char*);
        typedef int (*XCloseDisplayFunc)(void*);
        typedef int (*XQueryKeymapFunc)(void*, char*);
        
        static XOpenDisplayFunc pXOpenDisplay = (XOpenDisplayFunc)x11lib.resolve("XOpenDisplay");
        static XCloseDisplayFunc pXCloseDisplay = (XCloseDisplayFunc)x11lib.resolve("XCloseDisplay");
        static XQueryKeymapFunc pXQueryKeymap = (XQueryKeymapFunc)x11lib.resolve("XQueryKeymap");
        
        if (pXOpenDisplay && pXCloseDisplay && pXQueryKeymap) {
            void* display = pXOpenDisplay(nullptr);
            if (display) {
                char currentKeymap[32];
                pXQueryKeymap(display, currentKeymap);
                pXCloseDisplay(display);
                
                static char s_lastKeymap[32] = {0};
                static bool s_initialized = false;
                if (!s_initialized) {
                    memcpy(s_lastKeymap, currentKeymap, 32);
                    s_initialized = true;
                } else {
                    if (memcmp(s_lastKeymap, currentKeymap, 32) != 0) {
                        memcpy(s_lastKeymap, currentKeymap, 32);
                        reportActivity();
                    }
                }
            }
        }
    }
#endif
    
    // 重新获取一下（因为 reportActivity 可能修改了 manualIdle）
    idleMs = getSystemIdleTimeMs();
"""
        lines.insert(insert_idx, insert_code)
        break

with open('/home/test/screenSaver/IdleMonitor.cpp', 'w') as f:
    f.writelines(lines)
