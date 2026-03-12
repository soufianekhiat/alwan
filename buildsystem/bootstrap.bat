@echo off
REM Alwan Bootstrap Script
REM Builds Sharpmake and generates Visual Studio projects

echo ========================================
echo Alwan Bootstrap Script
echo ========================================

REM Resolve repo root (one level up from this script)
pushd "%~dp0.."
set REPO_ROOT=%CD%

REM Check if Sharpmake submodule exists
if not exist "extern\Sharpmake" (
    echo.
    echo Sharpmake submodule not found!
    echo Please run: git submodule update --init --recursive
    popd
    exit /b 1
)

REM Build Sharpmake
echo.
echo Step 1: Building Sharpmake...
echo ========================================
cd extern\Sharpmake

REM Check for dotnet
where dotnet >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: .NET SDK not found. Please install .NET 6.0 or later from https://dotnet.microsoft.com/download
    popd
    exit /b 1
)

REM Build Sharpmake.Application
echo Building Sharpmake.Application...
dotnet build Sharpmake.Application\Sharpmake.Application.csproj -c Release
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to build Sharpmake
    popd
    exit /b 1
)

cd "%REPO_ROOT%"

REM Copy Sharpmake artifacts to buildsystem/sharpmake
echo.
echo Step 2: Copying Sharpmake artifacts to buildsystem\sharpmake...
echo ========================================

if not exist "buildsystem\sharpmake" mkdir "buildsystem\sharpmake"

REM Copy all DLLs and EXE from Sharpmake build output
set SHARPMAKE_BIN=extern\Sharpmake\Sharpmake.Application\bin\x64\Release\net6.0

if not exist "%SHARPMAKE_BIN%" (
    echo Error: Sharpmake build output not found at %SHARPMAKE_BIN%
    popd
    exit /b 1
)

echo Copying from %SHARPMAKE_BIN% to buildsystem\sharpmake...
xcopy /Y /Q "%SHARPMAKE_BIN%\*.dll" "buildsystem\sharpmake\" >nul
xcopy /Y /Q "%SHARPMAKE_BIN%\*.exe" "buildsystem\sharpmake\" >nul
xcopy /Y /Q "%SHARPMAKE_BIN%\*.json" "buildsystem\sharpmake\" >nul

if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to copy Sharpmake artifacts
    popd
    exit /b 1
)

echo Sharpmake artifacts copied successfully!

REM Generate Visual Studio projects
echo.
echo Step 3: Generating Visual Studio projects...
echo ========================================
call "%~dp0generate_projects.bat"
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to generate projects
    popd
    exit /b 1
)

popd

echo.
echo ========================================
echo Bootstrap complete!
echo ========================================
echo.
echo You can now open the generated solution file:
echo   Alwan_vs2022_win64.sln
echo.
echo Or build from command line:
echo   msbuild Alwan_vs2022_win64.sln /p:Configuration=Debug
echo.
