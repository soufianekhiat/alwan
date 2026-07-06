#!/bin/sh
# Alwan Project Generation Script (POSIX twin of generate_projects.bat)
# Invokes Sharpmake to generate the Visual Studio projects.
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

echo "Generating Alwan Visual Studio projects..."

if [ ! -f "buildsystem/sharpmake/Sharpmake.Application.dll" ]; then
    echo "Error: Sharpmake not found in buildsystem/sharpmake/"
    echo "Please run buildsystem/bootstrap.sh first"
    exit 1
fi

# Invoke through the dotnet host so this works on every OS (the .exe apphost
# only exists on Windows; the .dll is the portable entry point).
dotnet buildsystem/sharpmake/Sharpmake.Application.dll "/sources('buildsystem/sharpmake/src/main.cs')"

echo "Projects generated successfully!"
