@echo off
setlocal enabledelayedexpansion

rem Check arguments
if "%1"=="" (
    echo USAGE: %~nx0 buildNum branch
    exit /b 1
)

set "basepath=%~dp0"
cd /d "%basepath%"
echo Basepath: %basepath%

rem Set architecture and build parameters
set ARCH=win32
set BUILD_NUM=%1
set branch=%2
echo Build number is: %BUILD_NUM%
echo Branch is: %branch%

rem Create output directories
if not exist "%basepath%build-%ARCH%" mkdir "%basepath%build-%ARCH%"
if not exist "%basepath%screenSaver-%ARCH%" mkdir "%basepath%screenSaver-%ARCH%"

echo "Preparing dependencies..."
@pushd "%basepath%"
setlocal
if not exist "%basepath%deps" mkdir "%basepath%deps"
endlocal
@popd

@pushd "%basepath%build-%ARCH%"
setlocal

rem Configure and Build with CMake
set BOOST_ROOT=C:\devtool\boost_1_87_0
set MSVC_ROOT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.29.30133

cmake -G "Visual Studio 17 2022" -T v142 -A Win32 .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_INSTALL_PREFIX="%basepath%screenSaver-%ARCH%"
if %ERRORLEVEL% NEQ 0 (
    echo CMake config failed!
    exit /b %ERRORLEVEL%
)

cmake --build . --config Release --verbose
if %ERRORLEVEL% NEQ 0 (
    echo CMake build failed!
    exit /b %ERRORLEVEL%
)
echo Build success!

cmake --install . --config Release --prefix "%basepath%screenSaver-%ARCH%"
if %ERRORLEVEL% NEQ 0 (
    echo CMake install failed!
    exit /b %ERRORLEVEL%
)

cd /d "%basepath%"
if not exist "%basepath%screenSaver-%ARCH%\screenSaver" mkdir "%basepath%screenSaver-%ARCH%\screenSaver"
xcopy "%basepath%screenSaver-%ARCH%\bin\*" "%basepath%screenSaver-%ARCH%\screenSaver\" /y /s
if exist "%basepath%screenSaver-%ARCH%\bin" rd /s /q "%basepath%screenSaver-%ARCH%\bin"
if exist "%basepath%screenSaver-%ARCH%\include" rd /s /q "%basepath%screenSaver-%ARCH%\include"
if exist "%basepath%screenSaver-%ARCH%\lib" rd /s /q "%basepath%screenSaver-%ARCH%\lib"

endlocal
@popd

echo "Packaging ZIP..."
@pushd "%basepath%"
setlocal

@REM set "ZIP_TOOL=C:\tool\7z.exe"
@REM if not exist "%ZIP_TOOL%" set "ZIP_TOOL=7z"

@REM "%ZIP_TOOL%" a -tzip -r "screenSaver-%BUILD_NUM%-%ARCH%.zip" "%basepath%screenSaver-%ARCH%\screenSaver\*"
@REM echo "ZIP build success!"

echo "Copying files for NSIS packaging..."
if not exist "%basepath%setup\windows\bin" mkdir "%basepath%setup\windows\bin"
xcopy "%basepath%screenSaver-%ARCH%\screenSaver\*" "%basepath%setup\windows\bin\" /y /s

echo "Calling setup\windows\build_win.bat for packaging..."
call "%basepath%setup\windows\build_win.bat" %BUILD_NUM% %branch%
if %ERRORLEVEL% NEQ 0 (
    echo Windows build_win.bat failed!
    exit /b %ERRORLEVEL%
)

endlocal
@popd

echo "Windows packaging complete!"
