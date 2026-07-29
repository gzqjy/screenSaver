import sys

with open('/home/test/screenSaver/ScreenSaverWidget.h', 'r') as f:
    lines = f.readlines()

out = []
for line in lines:
    if line.strip() == 'void showEvent(QShowEvent *event) override;':
        out.append(line)
        out.append('    void hideEvent(QHideEvent *event) override;\n')
        continue
    out.append(line)

with open('/home/test/screenSaver/ScreenSaverWidget.h', 'w') as f:
    f.writelines(out)
