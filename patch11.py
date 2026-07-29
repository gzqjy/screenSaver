import sys

with open('/home/test/screenSaver/main.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    out.append(line)
    if line.strip() == '} else {':
        out.append('        // 启动空闲检测\n')
        out.append('        idleMonitor.start();\n')

with open('/home/test/screenSaver/main.cpp', 'w') as f:
    f.writelines(out)
