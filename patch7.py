import sys

with open('/home/test/screenSaver/IdleMonitor.cpp', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if line.startswith('#include <QDateTime>'):
        lines.insert(i, '#include <QCursor>\n')
        break

for i, line in enumerate(lines):
    if line.startswith('    quint64 idleMs = getSystemIdleTimeMs();'):
        # insert the QCursor check right after this
        insert_code = """
    // 跨平台终极后备：通过轮询全局鼠标位置检测鼠标活动
    static QPoint s_lastMousePos = QCursor::pos();
    QPoint currentMousePos = QCursor::pos();
    if ((currentMousePos - s_lastMousePos).manhattanLength() > 5) {
        s_lastMousePos = currentMousePos;
        reportActivity(); // 强制覆盖系统空闲时间
        idleMs = getSystemIdleTimeMs(); // 重新获取
    }
"""
        lines.insert(i+1, insert_code)
        break

with open('/home/test/screenSaver/IdleMonitor.cpp', 'w') as f:
    f.writelines(lines)
