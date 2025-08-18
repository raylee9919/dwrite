@echo off
setlocal
cd /D "%~dp0"

where /q cl || (
    echo [ERROR]: "cl" not found - please run this from the MSVC x64 native tools command prompt.
    exit /b 1
)

set CFLAGS=/nologo /std:c++17 /Z7 /FC /utf-8 /DBUILD_DEBUG=1
set LFLAGS=/incremental:no
set LIBS=user32.lib gdi32.lib dwrite.lib d3d11.lib d3dcompiler.lib

if not exist build mkdir build
pushd build
call cl ..\src\main.cpp /Fe:main.exe %CFLAGS% /link %LFLAGS% %LIBS%
popd
