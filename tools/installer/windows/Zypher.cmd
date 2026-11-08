@echo off
rem Zypher launcher. Double-click to open the editor, or pass runtime options to
rem the command line runtime, for example:  Zypher.cmd --help
setlocal EnableExtensions EnableDelayedExpansion
title Zypher

set "APP=%~dp0"
set "EDITOR=%APP%bin\zypher_editor.exe"
set "RUNTIME=%APP%bin\zypher_runtime.exe"

if not "%~1"=="" goto :runtime

if exist "%EDITOR%" (
    start "" "%EDITOR%"
    goto :done
)
if not exist "%RUNTIME%" goto :missing
echo The editor is not part of this build; renderering the demo instead.
goto :demo

:runtime
if not exist "%RUNTIME%" goto :missing
"%RUNTIME%" %*
set "STATUS=!errorlevel!"
goto :done

:demo
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
