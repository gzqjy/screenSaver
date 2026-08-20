@echo off
setlocal enabledelayedexpansion

set "basepath=%~dp0"
cd /d "%basepath%"
echo Basepath: %basepath%

if "%1"=="" (
    echo Usage: build_win.bat build_num branch [output_dir]
    exit /b 1
)
set build_num=%1
set branch=%2
set "ROOT_DIR=%~3"
if "%ROOT_DIR%"=="" set "ROOT_DIR=%basepath%..\..\"
echo Build number is: %build_num%
echo Branch is: %branch%
echo Root directory is: %ROOT_DIR%

set version=1.1.0.%build_num%
echo Version is: %version%

if not exist "%basepath%bin" mkdir "%basepath%bin"
del /f /q "%basepath%bin\ver" 2>nul
set "CUR_TIME=%date:~0,4%/%date:~5,2%/%date:~8,2% %time:~0,8%"
echo Cur time: %CUR_TIME%

echo [info] > "%basepath%bin\ver"
echo build_version=%build_num% >> "%basepath%bin\ver"
echo build_time=%CUR_TIME% >> "%basepath%bin\ver"
echo version=%version% >> "%basepath%bin\ver"
echo vendor=sinoparasoft >> "%basepath%bin\ver"
echo branch=%branch% >> "%basepath%bin\ver"

echo { > "%basepath%bin\info"
echo "name": "screenSaver", >> "%basepath%bin\info"
echo "version": "%version%", >> "%basepath%bin\info"
echo "build_version": "%build_num%", >> "%basepath%bin\info"
echo "vendor": "sinoparasoft", >> "%basepath%bin\info"
echo "branch": "%branch%", >> "%basepath%bin\info"
echo "build_time": "%CUR_TIME%" >> "%basepath%bin\info"
echo } >> "%basepath%bin\info"

echo "Checking component..."
@pushd "%basepath%"
setlocal
set PUBLIC_URL=http://nexus.zkjs.com/repository/rawhostedrepo/public/windows/

rem 0. nsis
if not exist "%basepath%nsis" (
    echo "Downloading nsis..."
    call "%basepath%download_and_extract.bat" %PUBLIC_URL% nsis.7z .
)

rem 1. runtimelibrary
if exist "%basepath%download_and_extract.bat" (
    call "%basepath%download_and_extract.bat" %PUBLIC_URL% runtimelibrary.7z .\bin
)

endlocal
@popd

echo "Packaging with NSIS..."
@pushd "%basepath%"
setlocal

echo Version: %version%

if exist ".\sign_folder.bat" call .\sign_folder.bat .\bin\

set "MAKENSIS=%basepath%nsis\Unicode\makensis.exe"
if not exist "%MAKENSIS%" set "MAKENSIS=makensis.exe"

"%MAKENSIS%" /DMyVersion="%version%" "%basepath%install.nsi"
if %ERRORLEVEL% NEQ 0 (
    echo NSIS compilation failed!
    exit /b %ERRORLEVEL%
)

set INSTALLER_EXE=ScreenSaver_Setup.exe
if exist ".\sign_file.bat" (
    if exist ".\%INSTALLER_EXE%" call .\sign_file.bat ".\%INSTALLER_EXE%"
)

call "%basepath%update_and_compress.bat" %version% win32
if %ERRORLEVEL% NEQ 0 (
    echo update_and_compress.bat failed!
    exit /b %ERRORLEVEL%
)

if exist "%basepath%*.zip" copy /y "%basepath%*.zip" "%ROOT_DIR%\"
if exist "%basepath%ScreenSaver_Setup.exe" copy /y "%basepath%ScreenSaver_Setup.exe" "%ROOT_DIR%\"

endlocal
@popd
