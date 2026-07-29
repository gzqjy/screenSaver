import sys

with open('/home/test/screenSaver/ScreenSaverManager.cpp', 'r') as f:
    lines = f.readlines()

out = []
skip = 0
for line in lines:
    if skip > 0:
        skip -= 1
        continue
    if line.strip() == '// 前500ms防止误触':
        # we know this appears in checkGlobalInput and deactivateAll
        # we only want to remove it from deactivateAll.
        pass

out = []
in_deactivate = False
for line in lines:
    if line.startswith('void ScreenSaverManager::deactivateAll()'):
        in_deactivate = True
    
    if in_deactivate and line.strip() == '// 前500ms防止误触':
        skip = 3
    
    if skip > 0:
        skip -= 1
        continue
        
    out.append(line)
    
    if in_deactivate and line.startswith('}'):
        in_deactivate = False

with open('/home/test/screenSaver/ScreenSaverManager.cpp', 'w') as f:
    f.writelines(out)
