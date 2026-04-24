@echo off
setlocal

:: -------------------------------------------------------
:: Copy this file and fill in the two values below.
:: DISPLAY_NAME : human-readable name shown in output
:: SUB_PATH     : path to the submodule from repo root
::                e.g. third_party/mylib
:: -------------------------------------------------------
set DISPLAY_NAME=fastgltf
set SUB_PATH=third_party/fastgltf
:: -------------------------------------------------------

set REPO_ROOT=%~dp0..\..
set SUB_DIR=%REPO_ROOT%\%SUB_PATH:/=\%

echo Current %DISPLAY_NAME% pin:
git -C "%SUB_DIR%" describe --tags --always
echo.

if "%1"=="" (
    echo Updating %DISPLAY_NAME% to latest remote...
    git -C "%REPO_ROOT%" submodule update --remote %SUB_PATH%
) else (
    echo Pinning %DISPLAY_NAME% to %1...
    git -C "%SUB_DIR%" fetch --tags
    git -C "%SUB_DIR%" checkout %1
    git -C "%REPO_ROOT%" add %SUB_PATH%
)

echo.
echo New %DISPLAY_NAME% pin:
git -C "%SUB_DIR%" describe --tags --always
echo.
echo Run 'git commit' from the repo root to save this pin.
echo.
pause
