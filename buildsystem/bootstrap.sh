#!/bin/sh
# Alwan Bootstrap Script (POSIX twin of bootstrap.bat)
# Builds Sharpmake and generates the Visual Studio projects.
#
# Sharpmake runs anywhere .NET 6+ runs; the generated solution targets
# Visual Studio 2022, so it is buildable on Windows. On Linux/macOS the
# portable build path is CMake:  cmake -S . -B build && cmake --build build
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

echo "========================================"
echo "Alwan Bootstrap Script"
echo "========================================"

if [ ! -d "extern/Sharpmake" ]; then
    echo ""
    echo "Sharpmake submodule not found!"
    echo "Please run: git submodule update --init --recursive"
    exit 1
fi

echo ""
echo "Step 1: Building Sharpmake..."
echo "========================================"

if ! command -v dotnet >/dev/null 2>&1; then
    echo "Error: .NET SDK not found. Please install .NET 6.0 or later from https://dotnet.microsoft.com/download"
    exit 1
fi

dotnet build extern/Sharpmake/Sharpmake.Application/Sharpmake.Application.csproj -c Release

echo ""
echo "Step 2: Copying Sharpmake artifacts to buildsystem/sharpmake..."
echo "========================================"

mkdir -p buildsystem/sharpmake

SHARPMAKE_BIN="extern/Sharpmake/Sharpmake.Application/bin/x64/Release/net6.0"
if [ ! -d "$SHARPMAKE_BIN" ]; then
    # Non-Windows builds may drop the x64 platform folder
    SHARPMAKE_BIN="extern/Sharpmake/Sharpmake.Application/bin/Release/net6.0"
fi
if [ ! -d "$SHARPMAKE_BIN" ]; then
    echo "Error: Sharpmake build output not found"
    exit 1
fi

echo "Copying from $SHARPMAKE_BIN to buildsystem/sharpmake..."
cp -f "$SHARPMAKE_BIN"/*.dll buildsystem/sharpmake/
cp -f "$SHARPMAKE_BIN"/*.json buildsystem/sharpmake/ 2>/dev/null || true
cp -f "$SHARPMAKE_BIN"/*.exe buildsystem/sharpmake/ 2>/dev/null || true

echo "Sharpmake artifacts copied successfully!"

echo ""
echo "Step 3: Generating Visual Studio projects..."
echo "========================================"
"$SCRIPT_DIR/generate_projects.sh"

echo ""
echo "========================================"
echo "Bootstrap complete!"
echo "========================================"
echo ""
echo "Generated solution: Alwan_vs2022_win64.sln (build with Visual Studio / msbuild)."
echo "For a native build on this platform use CMake instead:"
echo "  cmake -S . -B build && cmake --build build"
