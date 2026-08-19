@echo off
setlocal

set PACKAGE_VERSION=%1
set ARCHITECT=%2

set JSON_FILE=abstract.json
set PACKAGE_NAME=ScreenSaver_Setup.exe
set ZIP_NAME=screenSaver_%PACKAGE_VERSION%_%ARCHITECT%.zip

set ZIP_TOOL=C:\tool\7z.exe
if not exist "%ZIP_TOOL%" set ZIP_TOOL=7z

"%ZIP_TOOL%" a "%ZIP_NAME%" "%PACKAGE_NAME%" "%JSON_FILE%"

if %ERRORLEVEL% EQU 0 (
    echo Created ZIP archive: %ZIP_NAME%
) else (
    echo Failed to create ZIP archive: %ZIP_NAME%
    exit /b 1
)

endlocal