@echo off
setlocal
if not exist "%~dp0..\data\build" call "%~dp0prebuild_data.bat"
set DIST=%~dp0..\dist\jianhuan
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"
copy /y "%~dp0..\build\src\platform\windows\jianhuan_tsf.dll" "%DIST%\"
copy /y "%~dp0..\build\src\platform\windows\jianhuan_register.exe" "%DIST%\"
if exist "%~dp0..\build\src\platform\windows\jianhuan_settings.exe" copy /y "%~dp0..\build\src\platform\windows\jianhuan_settings.exe" "%DIST%\"
if exist "%~dp0..\build\src\platform\windows\jianhuan_tray.exe" copy /y "%~dp0..\build\src\platform\windows\jianhuan_tray.exe" "%DIST%\"
copy /y "%~dp0..\src\platform\windows\jianhuan.ico" "%DIST%\"
if exist "%~dp0..\build-x86\src\platform\windows\jianhuan_register.exe" copy /y "%~dp0..\build-x86\src\platform\windows\jianhuan_register.exe" "%DIST%\jianhuan_register32.exe"
if exist "%~dp0..\build-x86\src\platform\windows\jianhuan_tsf.dll" copy /y "%~dp0..\build-x86\src\platform\windows\jianhuan_tsf.dll" "%DIST%\jianhuan_tsf32.dll"
robocopy "%~dp0..\data" "%DIST%\data" /E /XF jianhuan_phrases.dict.yaml pinyin_simp.dict.yaml /NFL /NDL /NJH /NJS
robocopy "%~dp0..\licenses" "%DIST%\licenses" /E /NFL /NDL /NJH /NJS
echo Packaged %DIST%
echo Register manually with: jianhuan_register.exe /register