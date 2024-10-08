@echo off
setlocal

:main
    rem push directory and change current directory
    pushd "%~dp0\Source\ThirdParty\assimp\assimp"

    rem assure that cmake command exists
    call :assureHasCommand cmake

    rem cmake
    call :assureExecute cmake -DASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT=OFF -DASSIMP_INSTALL=OFF -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
    call :assureExecute cmake --build . --config Release

    rem pop from stack
    popd
exit /b

:assureHasCommand
    rem check command existence
    where %1 >nul 2>nul

    rem if the command was not found
    if not %errorlevel% equ 0 (
        rem show error message
        echo ERROR: '%1' is not recognized as an internal or external command, operable program or batch file. 1>&2

        rem show path for check details
        setlocal enabledelayedexpansion
        echo PATH = !PATH!
        endlocal

        rem exit with command not found error
        exit 9009
    )
exit /b

:assureExecute
    setlocal

    rem generate escaped command
    set escapedcommand=%*
    set escapedcommand=%escapedcommand:"=\"%

    rem exectute command
    %*

    rem if has error
    if not %errorlevel% equ 0 (
        rem show error message
        echo ERROR: command "%escapedcommand%" failed. 1>&2

        rem show path for check details
        setlocal enabledelayedexpansion
        echo PATH = !PATH!
        endlocal

        rem exit with error
        exit %errorlevel%
    )
exit /b
