#!/bin/bash

set -e

# Set path explicitly
export PATH="/usr/local/bin:$PATH"

# Function to check if a command exists
assureHasCommand() {
    if ! command -v "$1" &> /dev/null; then
        echo "$1 is not recognized as a command."
        exit 9009
    fi
}

# Function to execute a command and check its result
assureExecute() {
    "$@"
    if [ $? -ne 0 ]; then
        echo "Command \"$*\" failed."
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
    assureExecute cmake CMakeLists.txt
    assureExecute cmake --build . --config Release
)
