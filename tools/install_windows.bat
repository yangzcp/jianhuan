@echo off
setlocal
set SRC=%~dp0..\dist\jianhuan
set TARGET=%LOCALAPPDATA%\Jianhuan
if not exist "%SRC%\jianhuan_tsf.dll" (
  echo Package first: tools\package_windows.bat
  exit /b 1
)
if not exist "%TARGET%" mkdir "%TARGET%"
xcopy /e /i /y "%SRC%" "%TARGET%" >nul
"%TARGET%\jianhuan_register.exe" /register
if errorlevel 1 exit /b 1
echo Installed to %TARGET%
echo Switch to Jianhuan in Windows input settings.