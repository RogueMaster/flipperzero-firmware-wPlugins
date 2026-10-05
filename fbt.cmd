@echo off

call "%~dp0scripts\toolchain\fbtenv.cmd" env || exit /b

set SCONS_EP=python -m SCons

if [%FBT_NO_SYNC%] == [] (
    if exist ".git" (
        call :sync_submodules_if_needed || exit /b
    ) else (
        echo .git not found, please clone repo with "git clone"
        exit /b 1
    )
)

set "SCONS_DEFAULT_FLAGS=--warn=target-not-built"
if not defined FBT_VERBOSE (
    set "SCONS_DEFAULT_FLAGS=%SCONS_DEFAULT_FLAGS% -Q"
)

%SCONS_EP% %SCONS_DEFAULT_FLAGS% %*
exit /b %ERRORLEVEL%

:sync_submodules_if_needed
setlocal EnableExtensions EnableDelayedExpansion

set "_FBT_SYNC_STAMP=.git\fbt_submodule_sync_head"
set "_FBT_CURRENT_HEAD="
set "_FBT_SYNCED_HEAD="
set "_FBT_NEEDS_SYNC="

for /f "delims=" %%H in ('git rev-parse HEAD 2^>nul') do set "_FBT_CURRENT_HEAD=%%H"
if not defined _FBT_CURRENT_HEAD (
    echo Failed to resolve Git HEAD
    endlocal & exit /b 1
)

if defined FBT_FORCE_SYNC (
    set "_FBT_NEEDS_SYNC=1"
) else if not exist "!_FBT_SYNC_STAMP!" (
    set "_FBT_NEEDS_SYNC=1"
) else (
    set /p "_FBT_SYNCED_HEAD="<"!_FBT_SYNC_STAMP!"
    if /I not "!_FBT_SYNCED_HEAD!"=="!_FBT_CURRENT_HEAD!" set "_FBT_NEEDS_SYNC=1"
)

rem A locally changed .gitmodules can alter paths/URLs without changing HEAD.
if not defined _FBT_NEEDS_SYNC (
    git diff --quiet -- .gitmodules >nul 2>&1
    if errorlevel 1 set "_FBT_NEEDS_SYNC=1"
)
if not defined _FBT_NEEDS_SYNC (
    git diff --cached --quiet -- .gitmodules >nul 2>&1
    if errorlevel 1 set "_FBT_NEEDS_SYNC=1"
)

rem Repair a missing submodule checkout without doing a recursive update on
rem every normal build.
if not defined _FBT_NEEDS_SYNC if exist ".gitmodules" (
    for /f "tokens=1,*" %%A in ('git config --file .gitmodules --get-regexp "^submodule\..*\.path$" 2^>nul') do (
        if not exist "%%B\.git" set "_FBT_NEEDS_SYNC=1"
    )
)

if not defined _FBT_NEEDS_SYNC (
    endlocal & exit /b 0
)

set "_FBT_CLONE_FLAGS=--jobs %NUMBER_OF_PROCESSORS%"
if defined FBT_GIT_SUBMODULE_SHALLOW set "_FBT_CLONE_FLAGS=!_FBT_CLONE_FLAGS! --depth 1"

git submodule --quiet sync --recursive
git submodule update --init --recursive !_FBT_CLONE_FLAGS!
if errorlevel 1 (
    echo Failed to update submodules, set FBT_NO_SYNC to skip
    endlocal & exit /b 1
)

>"!_FBT_SYNC_STAMP!" echo !_FBT_CURRENT_HEAD!
endlocal & exit /b 0
