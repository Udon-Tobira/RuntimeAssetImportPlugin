@echo off
setlocal

:main
    pushd "%~dp0\Source\ThirdParty\assimp\assimp"

    rem body
    cmake CMakeLists.txt
    cmake --build . --config Release

    popd
exit /b
