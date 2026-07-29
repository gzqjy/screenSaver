import sys

with open('/home/test/screenSaver/CMakeLists.txt', 'r') as f:
    lines = f.readlines()

out = []
in_x11_block = False
for line in lines:
    if line.strip() == 'if(X11_FOUND AND XSS_LIB)':
        out.append('    if(X11_FOUND)\n')
        out.append('        target_include_directories(ScreenSaver PRIVATE ${X11_INCLUDE_DIR})\n')
        out.append('        target_link_libraries(ScreenSaver PRIVATE ${X11_LIBRARIES})\n')
        out.append('        if(XSS_LIB)\n')
        out.append('            target_link_libraries(ScreenSaver PRIVATE ${XSS_LIB})\n')
        out.append('            add_definitions(-DHAVE_XSS)\n')
        out.append('            message(STATUS "X11 XScreenSaver extension: FOUND (X11+Wayland idle detection)")\n')
        out.append('        else()\n')
        out.append('            message(STATUS "X11 XScreenSaver extension: NOT FOUND (using X11 core + D-Bus idle detection)")\n')
        out.append('        endif()\n')
        in_x11_block = True
    elif in_x11_block and line.strip() == 'endif()':
        out.append(line)
        in_x11_block = False
    elif not in_x11_block:
        out.append(line)

with open('/home/test/screenSaver/CMakeLists.txt', 'w') as f:
    f.writelines(out)
