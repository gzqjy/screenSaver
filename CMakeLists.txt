cmake_minimum_required(VERSION 3.10)
project(ScreenSaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ===== 查找 Qt =====
find_package(Qt5 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt5_FOUND)
    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    set(QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets)
    # Linux: D-Bus 用于 Wayland 空闲检测
    if(UNIX AND NOT APPLE)
        find_package(Qt6 COMPONENTS DBus QUIET)
        if(Qt6DBus_FOUND)
            list(APPEND QT_LIBS Qt6::DBus)
        endif()
    endif()
else()
    set(QT_LIBS Qt5::Core Qt5::Gui Qt5::Widgets)
    if(UNIX AND NOT APPLE)
        find_package(Qt5 COMPONENTS DBus QUIET)
        if(Qt5DBus_FOUND)
            list(APPEND QT_LIBS Qt5::DBus)
        endif()
    endif()
endif()

# ============================================================
#  目标 1: ScreenSaver (Qt 屏保应用，跨平台)
# ============================================================
set(SCREENSAVER_SOURCES
    main.cpp
    ScreenSaverConfig.cpp
    IdleMonitor.cpp
    ImageManager.cpp
    ScreenSaverWidget.cpp
    ScreenSaverManager.cpp
)

set(SCREENSAVER_HEADERS
    ScreenSaverConfig.h
    IdleMonitor.h
    ImageManager.h
    ScreenSaverWidget.h
    ScreenSaverManager.h
)

add_executable(ScreenSaver WIN32 ${SCREENSAVER_SOURCES} ${SCREENSAVER_HEADERS})
target_link_libraries(ScreenSaver PRIVATE ${QT_LIBS})

# 平台特定配置
if(WIN32)
    target_link_libraries(ScreenSaver PRIVATE user32)
elseif(UNIX AND NOT APPLE)
    # XScreenSaver 扩展（可选，X11 环境下使用）
    find_package(X11 QUIET)
    find_library(XSS_LIB Xss QUIET)
    if(X11_FOUND AND XSS_LIB)
        target_compile_definitions(ScreenSaver PRIVATE HAVE_XSS=1)
        target_include_directories(ScreenSaver PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(ScreenSaver PRIVATE ${X11_LIBRARIES} ${XSS_LIB})
        message(STATUS "X11 XScreenSaver extension: FOUND (X11+Wayland idle detection)")
    else()
        message(STATUS "X11 XScreenSaver extension: NOT FOUND (using D-Bus idle detection only)")
    endif()
endif()

# ============================================================
#  目标 2: 守护进程/服务 (跨平台，不依赖 Qt)
# ============================================================
if(WIN32)
    # ----- Windows: ScreenSaverService.exe -----
    add_executable(ScreenSaverService
        service_main.cpp
        ScreenSaverService.cpp
        ScreenSaverService.h
    )
    target_link_libraries(ScreenSaverService PRIVATE
        advapi32 wtsapi32 userenv shlwapi
    )

elseif(UNIX AND NOT APPLE)
    # ----- Linux: ScreenSaverDaemon -----
    add_executable(ScreenSaverDaemon
        service_main.cpp
        ScreenSaverDaemon.cpp
        ScreenSaverDaemon.h
    )
    # 纯 POSIX，无额外依赖

endif()

# ============================================================
#  复制资源文件
# ============================================================
configure_file(
    ${CMAKE_SOURCE_DIR}/screensaver_config.json
    ${CMAKE_BINARY_DIR}/screensaver_config.json
    COPYONLY
)

if(WIN32)
    foreach(script setup_auto_login.bat setup_revert.bat)
        if(EXISTS ${CMAKE_SOURCE_DIR}/${script})
            configure_file(${CMAKE_SOURCE_DIR}/${script}
                           ${CMAKE_BINARY_DIR}/${script} COPYONLY)
        endif()
    endforeach()
endif()

# ============================================================
#  构建信息摘要
# ============================================================
message(STATUS "========================================")
message(STATUS "  ScreenSaver Build Configuration")
message(STATUS "  Qt version: ${Qt5_VERSION}${Qt6_VERSION}")
message(STATUS "  Platform:   ${CMAKE_SYSTEM_NAME}")
if(WIN32)
    message(STATUS "  Targets:    ScreenSaver.exe + ScreenSaverService.exe")
elseif(UNIX)
    message(STATUS "  Targets:    ScreenSaver + ScreenSaverDaemon")
endif()
message(STATUS "========================================")
