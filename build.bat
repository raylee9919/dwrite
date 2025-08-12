@echo off
setlocal
cd /D "%~dp0"

where /q cl || (
    echo [ERROR]: "cl" not found - please run this from the MSVC x64 native tools command prompt.
    exit /b 1
)

set CFLAGS=/nologo /std:c++17 /Z7 /FC
set LFLAGS=/incremental:no

if not exist build mkdir build
pushd build
::call cl ..\src\example1.cpp /Fe:example1.exe %CFLAGS% /link %LFLAGS%
::call cl ..\src\example2.cpp /Fe:example2.exe %CFLAGS% /link %LFLAGS%
::call cl ..\src\example3.cpp /Fe:example3.exe %CFLAGS% /link %LFLAGS%
call cl ..\src\example4.cpp /Fe:example4.exe %CFLAGS% /link %LFLAGS%
popd
