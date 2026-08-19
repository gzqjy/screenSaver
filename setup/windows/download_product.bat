@echo off
setlocal enabledelayedexpansion

set "package_name=%~1"
set "local_arch=%~2"
set "download_dir=%~3"
set "branch=%~4"
set "base_url=http://nexus.zkjs.com/repository/rawhostedrepo/product/%package_name%/%local_arch%"
set "version_file=ver.txt"

if not "%branch%"=="" (
    set "base_url=http://nexus.zkjs.com/repository/rawhostedrepo/product/%branch%/%package_name%/%local_arch%"
)

echo package_name=%package_name%
echo arch=%local_arch%
echo base_url=%base_url%

if "%package_name%"=="" (
    echo Usage: download_product ^<package_name^> ^<arch^>
    exit /b 1
)

if "%local_arch%"=="" (
    echo Usage: download_product ^<package_name^> ^<arch^>
    exit /b 1
)

echo Downloading version file from %base_url%/%version_file%
curl.exe -s "%base_url%/%version_file%" -o "%version_file%"
if %errorlevel% neq 0 (
    echo Failed to download version file.
    exit /b 1
)

:: 解析 build_number
for /f "tokens=* delims=" %%a in ('findstr /r "^[0-9]*" "%version_file%"') do set "build_number=%%a" & goto :parse_done
:parse_done

:: 去除 build_number 的前后和中间空格
set "build_number=%build_number: =%"

if "%build_number%"=="" (
    echo Failed to parse build number from version file.
    exit /b 1
)
echo Detected build number: %build_number%

set "product_file=%package_name%-%build_number%-%local_arch%.zip"
set "product_url=%base_url%/%build_number%/%product_file%"

echo Downloading product file: %product_file%...
echo Downloading from: %product_url%...
rem "c:\tool\curl.exe" -s "%product_url%" -o "%product_file%"
c:\tool\curl.exe -s -o "%product_file%" "%product_url%"
if %errorlevel% neq 0 (
    echo Failed to download product file.
    exit /b 1
)
echo Download complete: %product_file%

:: 使用 7z.exe 解压
c:/tool/7z.exe x "%product_file%" -o"%download_dir%" -y
rem "C:\tool\7z.exe" x "%tar_file%" -o"%target_dir%" -y
if %errorlevel% neq 0 (
    echo Failed to extract %product_file%.
    exit /b 1
) else (
    echo Extracted %product_file% to %download_dir%.
)

:: 删除下载的文件
del /f "%product_file%"

exit /b 0
