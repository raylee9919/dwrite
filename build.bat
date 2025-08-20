@echo off
setlocal
cd /D "%~dp0"

where /q cl || (
    echo [ERROR]: "cl" not found - please run this from the MSVC x64 native tools command prompt.
    exit /b 1
)

set CFLAGS=/nologo /std:c++17 /Z7 /W4 /FC /utf-8 /DBUILD_DEBUG=1 /wd4100 /wd4457
set LFLAGS=/incremental:no
set LIBS=user32.lib gdi32.lib dwrite.lib d3d11.lib d3dcompiler.lib d2d1.lib

if not exist build mkdir build
pushd build

:: Compile hlsl offline.
call fxc /nologo /T vs_5_0 /E vs_main /O3 /WX /Fh ../src/shader_vs.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../src/shader.hlsl
call fxc /nologo /T ps_5_0 /E ps_main /O3 /WX /Fh ../src/shader_ps.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../src/shader.hlsl

:: Compile main.cpp
call cl ..\src\main.cpp /Fe:main.exe %CFLAGS% /link %LFLAGS% %LIBS%

popd
