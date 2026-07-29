import sys

with open('/home/test/screenSaver/main.cpp', 'r') as f:
    lines = f.readlines()

out = []
skip = False
for line in lines:
    if line.strip() == 'if (immediateShow) {':
        out.append('    // 无论以何种模式启动，只要屏保被解散，都退回后台监控状态\n')
        out.append('    QObject::connect(&manager, &ScreenSaverManager::allDismissed,\n')
        out.append('                     &idleMonitor, &IdleMonitor::start);\n\n')
        out.append(line)
        continue
    
    if line.strip() == '// 当由守护进程启动时（锁屏之上），如果用户移动了鼠标导致屏保退出，':
        skip = True
        continue
        
    if skip:
        if line.strip() == 'qApp, &QCoreApplication::quit);':
            skip = False
        continue
        
    if line.strip() == '// 屏保完全退出 → 重新开始检测空闲':
        skip = True
        continue
        
    if skip:
        if line.strip() == '&idleMonitor, &IdleMonitor::start);':
            skip = False
        continue

    out.append(line)

with open('/home/test/screenSaver/main.cpp', 'w') as f:
    f.writelines(out)
