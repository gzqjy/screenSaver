@echo off
setlocal

set PACKAGE_VERSION=%1
set ARCHITECT=%2

set JSON_FILE=abstract.json
set PACKAGE_NAME=ScreenSaver_Setup.exe
set ZIP_NAME=screenSaver_%PACKAGE_VERSION%_%ARCHITECT%.zip

echo { > "%JSON_FILE%"
echo   "PACKAGE_NAME": "%PACKAGE_NAME%", >> "%JSON_FILE%"
echo   "PACKAGE_VERSION": "%PACKAGE_VERSION%", >> "%JSON_FILE%"
echo   "PACKAGE_DESCRIBE": "ScreenSaver Windows Release", >> "%JSON_FILE%"
echo   "ARCHITECT": "x86", >> "%JSON_FILE%"
echo   "OS_VERSION": "win" >> "%JSON_FILE%"
echo } >> "%JSON_FILE%"

echo Updated %JSON_FILE% successfully.

call compress.bat %PACKAGE_VERSION% %ARCHITECT%
if %ERRORLEVEL% NEQ 0 (
    exit /b %ERRORLEVEL%
)

endlocal