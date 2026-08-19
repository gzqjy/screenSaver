@echo off
setlocal enabledelayedexpansion


set base_url=%1
set file=%2
set target_dir=%3

:: 输入参数检查
if "%base_url%"=="" (
    echo Usage: download_and_extract ^<base_url^> ^<file_name^> ^<target_directory^>
    exit /b 1
)

if "%file%"=="" (
    echo Usage: download_and_extract ^<base_url^> ^<file_name^> ^<target_directory^>
    exit /b 1
)

if "%target_dir%"=="" (
    echo Usage: download_and_extract ^<base_url^> ^<file_name^> ^<target_directory^>
    exit /b 1
)

:: 创建目标目录
if not exist "%target_dir%" (
    mkdir "%target_dir%"
)

:: 下载文件
echo Downloading %file%...
echo url= %base_url%%file%
c:\tool\curl.exe -s -o %file% "%base_url%%file%"
if %ERRORLEVEL% neq 0 (
    echo Failed to download %file%.
    exit /b 1
)

echo Successfully downloaded %file%. Extracting...

:: 解压文件
"C:\tool\7z.exe" x %file% -o%target_dir% -y
if %ERRORLEVEL% neq 0 (
    echo Failed to extract %file%.
    exit /b 1
)

:: 如果有 .tar 文件，则继续解压 .tar 文件
set tar_file=%target_dir%\%file:.gz=%
if exist "%tar_file%" (
    echo Extracting %tar_file%...
    "C:\tool\7z.exe" x "%tar_file%" -o"%target_dir%" -y
    if %ERRORLEVEL% neq 0 (
        echo Failed to extract %tar_file%.
        exit /b 1
    )
    echo Extracted %tar_file% to %target_dir%.
	del %tar_file%
)

echo Extracted %file% to %target_dir%.

:: 删除下载的压缩包（可选）
del %file%

endlocal

