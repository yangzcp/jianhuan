@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set CMAKE_EXE="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA_EXE="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
%CMAKE_EXE% -S "%~dp0.." -B "%~dp0..\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=%NINJA_EXE% -DJIANHUAN_BUILD_WINDOWS_TSF=ON
if errorlevel 1 exit /b 1
%CMAKE_EXE% --build "%~dp0..\build" --target jianhuan_console jianhuan_core_test jianhuan_bench jianhuan_tsf jianhuan_register