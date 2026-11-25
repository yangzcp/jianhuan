@echo off
setlocal
set TARGET=%LOCALAPPDATA%\Jianhuan
if exist "%TARGET%\jianhuan_register.exe" (
  "%TARGET%\jianhuan_register.exe" /unregister
)
if exist "%TARGET%" rmdir /s /q "%TARGET%"
echo Uninstalled Jianhuan.