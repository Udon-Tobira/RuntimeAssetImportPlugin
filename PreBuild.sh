#!/bin/bash

set -e

# Set homebrew path explicitly
export PATH="$PATH:/opt/homebrew/bin:/opt/homebrew/sbin"

# Function to check if a command exists
assureHasCommand() {
    if ! command -v "$1" &> /dev/null; then
        echo "ERROR: $1 is not recognized as a command." 1>&2
        echo "PATH = $PATH"
        exit 9009
    fi
}

# Function to execute a command and check its result
assureExecute() {
    "$@"
    if [ $? -ne 0 ]; then
        echo "ERROR: Command \"$*\" failed." 1>&2
        echo "PATH = $PATH"
        exit $?
    fi
}

# Main script
(
    # Change to the target directory
    cd "$(dirname "$0")/Source/ThirdParty/assimp/assimp"

    # Ensure cmake command exists
    assureHasCommand cmake

    # Run cmake commands
    assureExecute cmake -DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF -DASSIMP_INSTALL=OFF  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    assureExecute cmake --build . --config Release
)
