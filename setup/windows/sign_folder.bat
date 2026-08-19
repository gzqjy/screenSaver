@echo off
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo error: no folder specified.
    exit /b 1
)

set "folder=%~1"


if not exist "%folder%" (
    echo erro: "%folder%" not exist.
    exit /b 1
)

echo sign: "%folder%"


for /R %folder% %%f in (*.exe *.dll) do (
	set "filename=%%~nxf"

    if /I "!filename!"=="npcap-0.96.exe" (
        echo skip: !filename!
    ) else if /I "!filename!"=="msvcp140.dll" (
        echo skip: !filename!
    ) else if /I "!filename!"=="vcruntime140.dll" (
        echo skip: !filename!
    ) else if /I "!filename!"=="ucrtbase.dll" (
        echo skip: !filename!
    ) else if /I "!filename!"=="Packet.dll" (
        echo skip: !filename!
    ) else if /I "!filename!"=="wpcap.dll" (
        echo skip: !filename!
    ) else if /I "!filename:~0,11!"=="api-ms-win-" (
        echo skip: !filename!
    ) else if /I "!filename!"=="VeraCryptExpander.exe" (
        echo skip: !filename!
    ) else if /I "!filename!"=="VeraCrypt.exe" (
        echo skip: !filename!
    ) else if /I "!filename!"=="VCF.exe" (
        echo skip: !filename!
    ) else if /I "!filename!"=="concrt140.dll" (
        echo skip: !filename!
    ) else (
        signtool verify /pa "%%f" >nul 2>nul

        if !errorlevel! equ 0 (
            echo [SKIP] Already signed: "%%f"
        ) else (
            echo sign: %%f
            signtool sign /sha1 "3636cda383ec91aa234f0e6dd8306ec729783ed2" /tr http://timestamp.digicert.com /td sha256 /fd sha256 "%%f"
            if errorlevel 1 (
                echo sign failed: %%f
            )
        )
        
    )
)

echo "all files signed successfully."

exit /b 0
