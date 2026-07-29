import sys

with open('/home/test/screenSaver/main.cpp', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    out.append(line)
    if line.strip() == '} else {':
        break

out.append('        // 启动空闲检测\n')
out.append('        idleMonitor.start();\n')
out.append('    }\n')
out.append('\n')
out.append('    qDebug() << "ScreenSaver application started";\n')
out.append('    qDebug() << "  Config:" << configPath;\n')
out.append('    qDebug() << "  Idle timeout (not logged in):" << config.idleTimeoutSeconds() << "seconds";\n')
out.append('    qDebug() << "  Idle timeout (logged in):" << config.loggedInIdleTimeoutSeconds() << "seconds";\n')
out.append('    qDebug() << "  Immediate show:" << immediateShow;\n')
out.append('\n')
out.append('    return app.exec();\n')
out.append('}\n')

with open('/home/test/screenSaver/main.cpp', 'w') as f:
    f.writelines(out)
