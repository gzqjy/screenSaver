import sys

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'r') as f:
    lines = f.readlines()

out = []
skip = False
for line in lines:
    if line.strip() == 'void ScreenSaverWidget::hideEvent(QHideEvent *event)':
        skip = True
    if skip and line.strip() == '}':
        skip = False
        continue
    if skip:
        continue
    
    if line.strip() == 'QWidget::showEvent(event);' and 'void ScreenSaverWidget::showEvent' not in ''.join(out[-5:]):
        # spurious remaining
        continue
        
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverWidget.cpp', 'w') as f:
    f.writelines(out)
