import sys

with open('/home/test/screenSaver/IdleMonitor.cpp', 'r') as f:
    lines = f.readlines()

# 1. Move the QCursor and XQueryKeymap logic into getSystemIdleTimeMs
# 2. Fix the first call of proc interrupts

out = []
in_check_idle = False
skip = False

extract_code = []

for line in lines:
    if line.startswith('    // 跨平台终极后备：通过轮询全局鼠标位置检测鼠标活动'):
        in_check_idle = True
        skip = True
    
    if in_check_idle and line.strip() == 'idleMs = getSystemIdleTimeMs();':
        skip = False
        in_check_idle = False
        continue
        
    if skip:
        extract_code.append(line)
        continue
        
    if line.startswith('quint64 IdleMonitor::getSystemIdleTimeMs()'):
        out.append(line)
        out.append('{\n')
        out.append('    quint64 sysIdle = getSystemIdleTimeMsInternal();\n')
        
        # inject the extracted code here
        out.append('    // 跨平台终极后备：通过轮询全局鼠标位置检测鼠标活动\n')
        out.append('    static QPoint s_lastMousePos = QCursor::pos();\n')
        out.append('    QPoint currentMousePos = QCursor::pos();\n')
        out.append('    if ((currentMousePos - s_lastMousePos).manhattanLength() > 5) {\n')
        out.append('        s_lastMousePos = currentMousePos;\n')
        out.append('        reportActivity();\n')
        out.append('    }\n')
        out.append('#ifdef Q_OS_LINUX\n')
        out.append('    static QLibrary x11lib("X11");\n')
        out.append('    if (x11lib.load()) {\n')
        out.append('        typedef void* (*XOpenDisplayFunc)(const char*);\n')
        out.append('        typedef int (*XCloseDisplayFunc)(void*);\n')
        out.append('        typedef int (*XQueryKeymapFunc)(void*, char*);\n')
        out.append('        static XOpenDisplayFunc pXOpenDisplay = (XOpenDisplayFunc)x11lib.resolve("XOpenDisplay");\n')
        out.append('        static XCloseDisplayFunc pXCloseDisplay = (XCloseDisplayFunc)x11lib.resolve("XCloseDisplay");\n')
        out.append('        static XQueryKeymapFunc pXQueryKeymap = (XQueryKeymapFunc)x11lib.resolve("XQueryKeymap");\n')
        out.append('        if (pXOpenDisplay && pXCloseDisplay && pXQueryKeymap) {\n')
        out.append('            void* display = pXOpenDisplay(nullptr);\n')
        out.append('            if (display) {\n')
        out.append('                char currentKeymap[32];\n')
        out.append('                pXQueryKeymap(display, currentKeymap);\n')
        out.append('                pXCloseDisplay(display);\n')
        out.append('                static char s_lastKeymap[32] = {0};\n')
        out.append('                static bool s_initialized = false;\n')
        out.append('                if (!s_initialized) {\n')
        out.append('                    memcpy(s_lastKeymap, currentKeymap, 32);\n')
        out.append('                    s_initialized = true;\n')
        out.append('                } else {\n')
        out.append('                    if (memcmp(s_lastKeymap, currentKeymap, 32) != 0) {\n')
        out.append('                        memcpy(s_lastKeymap, currentKeymap, 32);\n')
        out.append('                        reportActivity();\n')
        out.append('                    }\n')
        out.append('                }\n')
        out.append('            }\n')
        out.append('        }\n')
        out.append('    }\n')
        out.append('#endif\n')
        
        out.append('    qint64 manualIdle = QDateTime::currentMSecsSinceEpoch() - s_manualActivityTime;\n')
        out.append('    if (manualIdle >= 0 && static_cast<quint64>(manualIdle) < sysIdle) {\n')
        out.append('        return static_cast<quint64>(manualIdle);\n')
        out.append('    }\n')
        out.append('    return sysIdle;\n')
        out.append('}\n')
        
        # skip the old body of getSystemIdleTimeMs
        skip_old_body = True
        continue

    if 'skip_old_body' in locals() and skip_old_body:
        if line.startswith('}'):
            skip_old_body = False
        continue

    # Fix the proc interrupts first call bug
    if 's_lastActivityTimestamp = now;' in line and 'return 0;' in ''.join(out[-2:]):
        # wait, we need to match the specific block
        pass
        
    out.append(line)

# Now fix the proc interrupts block
for i, line in enumerate(out):
    if line.strip() == 'if (s_lastInterruptCount < 0) {':
        out[i+2] = '        s_lastActivityTimestamp = now - 1000000;\n'
        out[i+3] = '        return 1000000;\n'

with open('/home/test/screenSaver/IdleMonitor.cpp', 'w') as f:
    f.writelines(out)

