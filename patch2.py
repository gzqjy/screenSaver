import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if line.startswith('#include <QMouseEvent>'):
        lines.insert(i, '#include <QDateTime>\n')
        break

for i, line in enumerate(lines):
    if line.strip() == 'm_mouseInitialized = false;':
        lines.insert(i+1, '    m_activationTime = QDateTime::currentMSecsSinceEpoch();\n')
        break

for i, line in enumerate(lines):
    if line.startswith('void ScreenSaverWidget::keyPressEvent(QKeyEvent * /*event*/)'):
        lines.insert(i+2, '    if (QDateTime::currentMSecsSinceEpoch() - m_activationTime < 500) return;\n')
        break

for i, line in enumerate(lines):
    if line.startswith('void ScreenSaverWidget::mousePressEvent(QMouseEvent * /*event*/)'):
        lines.insert(i+2, '    if (QDateTime::currentMSecsSinceEpoch() - m_activationTime < 500) return;\n')
        break

for i, line in enumerate(lines):
    if line.startswith('void ScreenSaverWidget::mouseMoveEvent(QMouseEvent *event)'):
        insert_idx = i + 2
        lines.insert(insert_idx, '    if (QDateTime::currentMSecsSinceEpoch() - m_activationTime < 500) {\n')
        lines.insert(insert_idx+1, '        m_lastMousePos = event->pos();\n')
        lines.insert(insert_idx+2, '        m_mouseInitialized = true;\n')
        lines.insert(insert_idx+3, '        return;\n')
        lines.insert(insert_idx+4, '    }\n\n')
        break

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'w') as f:
    f.writelines(lines)
