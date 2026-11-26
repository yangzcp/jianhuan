@echo off
setlocal
set DEV=%~dp0..
set REPO=%DEV%\..\jianhuan-repo
set MSG=%~1
if "%MSG%"=="" set MSG=update source and installer

robocopy "%DEV%\src" "%REPO%\src" /MIR /NFL /NDL /NJH /NJS >nul
robocopy "%DEV%\tests" "%REPO%\tests" /MIR /NFL /NDL /NJH /NJS >nul
robocopy "%DEV%\tools" "%REPO%\tools" /MIR /NFL /NDL /NJH /NJS >nul
robocopy "%DEV%\cmake" "%REPO%\cmake" /MIR /NFL /NDL /NJH /NJS >nul
robocopy "%DEV%\licenses" "%REPO%\licenses" /MIR /NFL /NDL /NJH /NJS >nul
robocopy "%DEV%\data" "%REPO%\data" /MIR /XD build /NFL /NDL /NJH /NJS >nul
robocopy "%DEV%\third_party" "%REPO%\third_party" /MIR /XD .git /NFL /NDL /NJH /NJS >nul
copy /y "%DEV%\CMakeLists.txt" "%REPO%\" >nul
copy /y "%DEV%\README.md" "%REPO%\" >nul
copy /y "%DEV%\README.en.md" "%REPO%\" >nul
copy /y "%DEV%\.gitignore" "%REPO%\" >nul
if not exist "%REPO%\dist" mkdir "%REPO%\dist"
if exist "%DEV%\dist\JianhuanSetup.exe" copy /y "%DEV%\dist\JianhuanSetup.exe" "%REPO%\dist\" >nul

git -C "%REPO%" add -A
git -C "%REPO%" -c user.name=yangzcp -c user.email=yangzcp@users.noreply.github.com commit -m "%MSG%"
git -C "%REPO%" push origin main
git -C "%REPO%" push gitee main
endlocal