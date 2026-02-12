@echo off
REM Alwan Project Generation Script
REM Invokes Sharpmake to generate Visual Studio projects

REM Resolve repo root (one level up from this script)
pushd "%~dp0.."
set REPO_ROOT=%CD%

echo Generating Alwan Visual Studio projects...

if not exist "buildsystem\sharpmake\Sharpmake.Application.exe" (
    echo Error: Sharpmake not found in buildsystem\sharpmake\
    echo Please run buildsystem\bootstrap.bat first
    popd
    exit /b 1
)

.\buildsystem\sharpmake\Sharpmake.Application.exe /sources('buildsystem/sharpmake/src/main.cs')

if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to generate projects
    popd
    exit /b 1
)

popd

echo Projects generated successfully!
