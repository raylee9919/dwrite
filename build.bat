@echo off
setlocal
cd /D "%~dp0"

:: Unpack Arguments.
for %%a in (%*) do set "%%a=1"

:: Clone codebase from git ::
set codebase_repo="https://github.com/raylee9919/codebase.git"
set codebase_dir=codebase
if "%pull%"=="1" (
    if not exist %codebase_dir% (
        echo Cloning codebase...
        git clone %codebase_repo% %codebase_dir%
    ) else ( 
        echo Updating codebase...
        pushd %codebase_dir%
        git stash
        git pull
        popd
    )
)

if not exist build mkdir build
pushd build

set src_dir=/I../codebase/src /I../src
set CFLAGS=/nologo /std:c++17 /Od /Z7 /W4 /FC /utf-8 %src_dir% /DBUILD_DEBUG=1 /wd4100 /wd4457 /wd4200 /wd4505 /wd4201 /wd4042
set LFLAGS=/incremental:no
set libs=gdi32.lib

:: Compile HLSL offline.
if "%hlsl%"=="1" (
    if not exist ../src/shaders mkdir ../src/shaders
    call fxc /nologo /T vs_5_0 /E vs_main /O3 /WX /Fh ../src/shaders/shader_vs.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../src/hlsl/shader.hlsl
    call fxc /nologo /T ps_5_0 /E ps_main /O3 /WX /Fh ../src/shaders/shader_ps.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../src/hlsl/shader.hlsl

    call fxc /nologo /T vs_5_0 /E panel_vs_main /O3 /WX /Fh ../src/shaders/panel_vs.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../src/hlsl/panel.hlsl
    call fxc /nologo /T ps_5_0 /E panel_ps_main /O3 /WX /Fh ../src/shaders/panel_ps.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv ../src/hlsl/panel.hlsl
)

:: Compile main.cpp
call cl ..\src\main.cpp /Fe:main.exe %CFLAGS% /link %LFLAGS% %libs%

popd
