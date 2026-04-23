@echo off
REM Build pupil_detect as a Windows DLL for the Python simulator.
REM Requires MinGW gcc on PATH (C:\mingw64\bin).

setlocal
set HERE=%~dp0
set CORE=%HERE%..\Application\STM32N6570-DK\Core
set OUT=%HERE%pupil_detect.dll

gcc -O2 -Wall -shared -o "%OUT%" ^
    -I "%CORE%\Inc" ^
    -I "%HERE%" ^
    "%HERE%sim_api.c" ^
    "%CORE%\Src\pupil_detect.c"

if errorlevel 1 (
    echo.
    echo *** BUILD FAILED ***
    exit /b 1
)

echo.
echo Built: %OUT%
endlocal
