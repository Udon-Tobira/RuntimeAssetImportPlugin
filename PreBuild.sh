#!/bin/bash

set -e

# Set path explicitly
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:$PATH"

# Function to check if a command exists
assureHasCommand() {
    if ! command -v "$1" &> /dev/null; then
        echo "ERROR: $1 is not recognized as a command." 1>&2
        exit 9009
    fi
}

# Function to execute a command and check its result
assureExecute() {
    "$@"
    if [ $? -ne 0 ]; then
        echo "ERROR: Command \"$*\" failed." 1>&2
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
    assureExecute cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    assureExecute cmake --build . --config Release
)
