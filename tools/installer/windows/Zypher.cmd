@echo off
rem Zypher launcher. Double-click to render the demo and read the report, or
rem pass runtime options, for example:  Zypher.cmd --help
setlocal EnableExtensions EnableDelayedExpansion
title Zypher

set "APP=%~dp0"
set "RUNTIME=%APP%bin\zypher_runtime.exe"
if not exist "%RUNTIME%" goto :missing

if not "%~1"=="" (
    "%RUNTIME%" %*
    set "STATUS=!errorlevel!"
    goto :done
)

set "OUT=%LOCALAPPDATA%\Zypher\artifacts"
if "%LOCALAPPDATA%"=="" set "OUT=%APP%artifacts"
set "SCENE=%APP%share\zypher\samples\demo\scene.zscene"
set "PACK=%APP%share\zypher\samples\demo\dist\demo.zpkg"

echo.
"%RUNTIME%" --version
echo.
echo   Rendering the demo scene (1280x720, 2 frames, 120 simulation steps)...
echo.

set "ARGS=--output "%OUT%" --width 1280 --height 720 --frames 2 --steps 120 --dump-frame"
if exist "%PACK%" set "ARGS=--pack "%PACK%" !ARGS!"
if exist "%SCENE%" set "ARGS=--scene "%SCENE%" !ARGS!"
"%RUNTIME%" !ARGS!
set "STATUS=!errorlevel!"

if not "!STATUS!"=="0" (
    echo.
    echo   Zypher exited with code !STATUS!.
    echo   See "%OUT%\zypher.log" for details.
    goto :hold
)

echo.
echo   Frame and report written to:
echo     %OUT%\frame.png
echo     %OUT%\report.json
echo.
echo   Next steps:
echo     Zypher.cmd --help
echo     Zypher.cmd --list-assets --pack "%PACK%"
echo     Zypher.cmd --scene ^<scene^> --pack ^<pack^> --output ^<dir^>

:hold
echo.
pause
goto :done

:missing
echo.
echo   Zypher could not start: bin\zypher_runtime.exe is missing.
echo   Download a fresh copy from https://github.com/itriedcoding/Zypher/releases
echo.
pause
set "STATUS=1"

:done
if not defined STATUS set "STATUS=0"
endlocal & exit /b %STATUS%
