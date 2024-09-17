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

# Function to remove version numbers from dylib file name
removeVersion() {
    sed -E 's/\.[0-9]+//g'
}

# Main script
(
    # Ensure cmake command exists
    assureHasCommand cmake


    ######### Make assimp #########

    # Change to the target directory
    cd "$(dirname "$0")/Source/ThirdParty/assimp/assimp"

    # Run cmake commands
    # assureExecute cmake -DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF -DASSIMP_INSTALL=OFF  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    assureExecute cmake -DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF -DASSIMP_INSTALL=OFF  -DCMAKE_OSX_ARCHITECTURES="arm64" -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    assureExecute cmake --build . --config Release

    ######### change assimp alias dylib to real dylib ########

    # Change to the library directory
    cd bin

    # for all *.dylib files in bin directory
    for lib_name in *.dylib; do
        lib_name_without_version=$(echo "$lib_name" | removeVersion)
        # change install path to itself
        install_name_tool -id "@rpath/$lib_name_without_version" "$lib_name"
    done

    # for all .*\.dylib or .*\..*\.dylib alias files in lib directory
    for alias_file_relpath in $(find . -type l); do
        # remove it
        rm $alias_file_relpath
    done

    # for all real dylib files
    for lib_name_with_version in *.dylib; do
        # rename it to a name that doesn't include the version number
        mv "$lib_name_with_version" "$(echo $lib_name_with_version | removeVersion)"
    done
)
