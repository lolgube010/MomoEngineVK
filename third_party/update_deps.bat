@echo off
setlocal enabledelayedexpansion

:: -------------------------------------------------------
:: Submodule list — add new entries here
:: -------------------------------------------------------
set DEP_COUNT=3

set DEP_NAME_1=fastgltf
set DEP_PATH_1=third_party/fastgltf

set DEP_NAME_2=fmt
set DEP_PATH_2=third_party/fmt

set DEP_NAME_3=tracy
set DEP_PATH_3=third_party/tracy
:: -------------------------------------------------------
::
:: Usage:
::   update_deps.bat          — menu, update selected to latest remote
::   update_deps.bat <tag>    — menu, pin selected to <tag>
::
:: -------------------------------------------------------

set REPO_ROOT=%~dp0..
set "TAG=%~1"

echo.
echo  Submodule updater
echo  -----------------
echo.
for /l %%i in (1,1,%DEP_COUNT%) do (
    set "SUB_DIR=!REPO_ROOT!\!DEP_PATH_%%i:/=\!"
    for /f "tokens=*" %%t in ('git -C "!SUB_DIR!" describe --tags --always 2^>nul') do (
        echo  [%%i] !DEP_NAME_%%i! ^(currently: %%t^)
    )
)
echo  [A] All of the above
echo  [Q] Quit
echo.
if not "%TAG%"=="" echo  Tag to pin: %TAG%
echo.

set /p "CHOICE=Select: "
echo.

if /i "%CHOICE%"=="q" goto :EOF
if /i "%CHOICE%"=="a" (
    for /l %%i in (1,1,%DEP_COUNT%) do call :do_update %%i
    goto :done
)

set "VALID=0"
for /l %%i in (1,1,%DEP_COUNT%) do if "%CHOICE%"=="%%i" set "VALID=1"
if "%VALID%"=="0" ( echo Invalid choice. & goto :EOF )

call :do_update %CHOICE%
goto :done

:: ---

:do_update
set "NAME=!DEP_NAME_%~1!"
set "SUB_PATH=!DEP_PATH_%~1!"
set "SUB_DIR=!REPO_ROOT!\!SUB_PATH:/=\!"

echo [!NAME!] current pin:
git -C "!SUB_DIR!" describe --tags --always

if "!TAG!"=="" (
    echo [!NAME!] updating to latest remote...
    git -C "!REPO_ROOT!" submodule update --remote !SUB_PATH!
) else (
    echo [!NAME!] pinning to !TAG!...
    git -C "!SUB_DIR!" fetch --tags
    git -C "!SUB_DIR!" checkout !TAG!
    git -C "!REPO_ROOT!" add !SUB_PATH!
)

echo [!NAME!] new pin:
git -C "!SUB_DIR!" describe --tags --always
echo.
goto :EOF

:: ---

:done
echo Run 'git commit' from the repo root to save changes.
echo.
pause
