@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%~dp0package_windows.bat"
node "%~dp0make_setup_resources.js"
if not exist "%~dp0..\build\setup" mkdir "%~dp0..\build\setup"
rc /nologo /fo "%~dp0..\build\setup_payload.res" "%~dp0..\build\setup_payload.rc"
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE "%~dp0setup.cpp" /Fo"%~dp0..\build\setup\\" /Fe"%~dp0..\dist\JianhuanSetup.exe" /link "%~dp0..\build\setup_payload.res" user32.lib shell32.lib advapi32.lib ole32.lib gdi32.lib /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup /MANIFEST:EMBED /MANIFESTINPUT:"%~dp0setup.manifest"
if errorlevel 1 exit /b 1
echo Built dist\JianhuanSetup.exe