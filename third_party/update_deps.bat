@echo off
setlocal enabledelayedexpansion

:: -------------------------------------------------------
:: Submodule list — add new entries here
:: -------------------------------------------------------
set DEP_COUNT=5

set DEP_NAME_1=fastgltf
set DEP_PATH_1=third_party/fastgltf

set DEP_NAME_2=fmt
set DEP_PATH_2=third_party/fmt

set DEP_NAME_3=imgui
set DEP_PATH_3=third_party/imgui

set DEP_NAME_4=tracy
set DEP_PATH_4=third_party/tracy

set DEP_NAME_5=vkbootstrap
set DEP_PATH_5=third_party/vkbootstrap

:: -------------------------------------------------------
:: Single-file list — add new entries here
:: FILE_URL_x  : raw URL with REFHERE where the branch/commit goes
:: FILE_REF_x  : default branch or tag
:: FILE_DST_x  : destination path relative to repo root
:: -------------------------------------------------------
set FILE_COUNT=2

set FILE_NAME_1=stb_image
set FILE_URL_1=https://raw.githubusercontent.com/nothings/stb/REFHERE/stb_image.h
set FILE_REF_1=master
set FILE_DST_1=third_party/stb_image/stb_image.h

set FILE_NAME_2=renderdoc_app
set FILE_URL_2=https://raw.githubusercontent.com/baldurk/renderdoc/REFHERE/renderdoc/api/app/renderdoc_app.h
set FILE_REF_2=v1.x
set FILE_DST_2=third_party/renderdoc_app/renderdoc_app.h

:: -------------------------------------------------------
::
:: Usage:
::   update_deps.bat          — menu, update selected to latest
::   update_deps.bat <ref>    — menu, pin submodule to <ref>,
::                              or download single-file at <ref>
::
:: -------------------------------------------------------

set REPO_ROOT=%~dp0..
set "TAG=%~1"

:: Offset single-file menu numbers past the submodule count
set /a FILE_OFFSET=%DEP_COUNT%

echo.
echo  Dependency updater
echo  ------------------
echo.
echo  -- Submodules --
for /l %%i in (1,1,%DEP_COUNT%) do (
    set "SUB_DIR=!REPO_ROOT!\!DEP_PATH_%%i:/=\!"
    for /f "tokens=*" %%t in ('git -C "!SUB_DIR!" describe --tags --always 2^>nul') do (
        echo  [%%i] !DEP_NAME_%%i! ^(currently: %%t^)
    )
)
echo.
echo  -- Single files --
for /l %%i in (1,1,%FILE_COUNT%) do (
    set /a MENU_IDX=%%i+%FILE_OFFSET%
    set "DST_PATH=!REPO_ROOT!\!FILE_DST_%%i:/=\!"
    if exist "!DST_PATH!" (
        echo  [!MENU_IDX!] !FILE_NAME_%%i! ^(default ref: !FILE_REF_%%i!^)
    ) else (
        echo  [!MENU_IDX!] !FILE_NAME_%%i! ^(not present^)
    )
)
echo.
echo  [A] All of the above
echo  [Q] Quit
echo.
if not "%TAG%"=="" echo  Ref/tag: %TAG%
echo.

set /p "CHOICE=Select: "
echo.

if /i "%CHOICE%"=="q" goto :EOF
if /i "%CHOICE%"=="a" (
    for /l %%i in (1,1,%DEP_COUNT%) do call :do_submodule %%i
    for /l %%i in (1,1,%FILE_COUNT%) do call :do_file %%i
    goto :done
)

:: Check submodule range
set "VALID=0"
for /l %%i in (1,1,%DEP_COUNT%) do if "%CHOICE%"=="%%i" set "VALID=1"
if "%VALID%"=="1" ( call :do_submodule %CHOICE% & goto :done )

:: Check single-file range
for /l %%i in (1,1,%FILE_COUNT%) do (
    set /a MENU_IDX=%%i+%FILE_OFFSET%
    if "%CHOICE%"=="!MENU_IDX!" ( call :do_file %%i & goto :done )
)

echo Invalid choice.
goto :EOF

:: ---

:do_submodule
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

:do_file
set "NAME=!FILE_NAME_%~1!"
set "BASE_URL=!FILE_URL_%~1!"
set "DEFAULT_REF=!FILE_REF_%~1!"
set "DST=!REPO_ROOT!\!FILE_DST_%~1:/=\!"

set "REF=!DEFAULT_REF!"
if not "!TAG!"=="" set "REF=!TAG!"

set "URL=!BASE_URL:REFHERE=!REF!!"

echo [!NAME!] downloading from ref: !REF!
curl -fsSL -o "!DST!" "!URL!"
if errorlevel 1 (
    echo [!NAME!] ERROR: download failed.
) else (
    echo [!NAME!] updated successfully.
)
echo.
goto :EOF

:: ---

:done
echo Run 'git commit' from the repo root to save changes.
echo.
pause
