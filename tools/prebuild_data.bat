@echo off
setlocal
set PREBUILD_USER=%~dp0..\build\prebuilt_user
if exist "%PREBUILD_USER%" rmdir /s /q "%PREBUILD_USER%"
if exist "%~dp0..\data\build" rmdir /s /q "%~dp0..\data\build"
"%~dp0..\build\jianhuan_bench.exe" "%~dp0..\data" "%PREBUILD_USER%" jianhuan_pinyin
if errorlevel 1 exit /b 1
for /d %%P in ("%PREBUILD_USER%\proc_*") do (
  if exist "%%P\build" xcopy /e /i /y "%%P\build" "%~dp0..\data\build"
)
echo Prebuilt data ready: data\build