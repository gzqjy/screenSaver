@echo off
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo "error no file specified".
    exit /b 1
)

set "file=%~1"


if not exist "!file!" (
    echo error: file !file! not exist.
    exit /b 1
)

signtool verify /pa "!file!" >nul 2>nul

if !errorlevel! equ 0 (
    echo [SKIP] Already signed: "!file!"
) else (
    echo [SIGNING] !file!
    signtool sign /sha1 "3636cda383ec91aa234f0e6dd8306ec729783ed2" /tr http://timestamp.digicert.com /td sha256 /fd sha256 "!file!"
    
    if errorlevel 1 (
        echo [ERROR] Sign failed: "!file!"
    ) else (
        echo [SUCCESS] Signed: "!file!"
    )
)

exit /b 0
