@echo off
setlocal
call "%~dp0build_windows_arch.bat" x64
if errorlevel 1 exit /b 1
"%~dp0..\build\jianhuan_core_test.exe" "%~dp0..\data"
if errorlevel 1 exit /b 1
call "%~dp0build_windows_arch.bat" x86
if errorlevel 1 exit /b 1
"%~dp0..\build-x86\jianhuan_core_test.exe" "%~dp0..\data"