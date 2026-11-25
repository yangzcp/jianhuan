@echo off
setlocal
set ARCH=%1
if "%ARCH%"=="" set ARCH=x64
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" %ARCH%
set CMAKE_EXE="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set NINJA_EXE="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if "%ARCH%"=="x64" (
  set DEPS=%~dp0..\.deps\vcpkg\x64-windows-static
  set BUILD=%~dp0..\build
) else if "%ARCH%"=="x86" (
  set DEPS=%~dp0..\.deps\vcpkg\x86-windows-static
  set BUILD=%~dp0..\build-x86
) else (
  echo Unsupported arch: %ARCH%
  exit /b 1
)
%CMAKE_EXE% -S "%~dp0.." -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=%NINJA_EXE% -DJIANHUAN_DEPS_PREFIX="%DEPS%" -DJIANHUAN_BUILD_WINDOWS_TSF=ON
if errorlevel 1 exit /b 1
%CMAKE_EXE% --build "%BUILD%" --target jianhuan_console jianhuan_core_test jianhuan_bench jianhuan_tsf jianhuan_register jianhuan_settings jianhuan_tray