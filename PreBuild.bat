@echo off
setlocal

:main
    rem push directory and change current directory
    pushd "%~dp0\Source\ThirdParty\assimp\assimp"

    rem assure that cmake command exists
    call :assureHasCommand cmake

    rem cmake
    call :assureExecute cmake CMakeLists.txt
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
        echo '%1' is not recognized as an internal or external command, operable program or batch file.

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
        echo command "%escapedcommand%" failed.

        rem exit with error
        exit %errorlevel%
    )
exit /b
