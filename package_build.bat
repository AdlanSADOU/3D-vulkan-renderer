@echo off
setlocal

set "source=%~dp0bin\Release"
set "destination=%~dp0Shipping"

if not exist "%destination%" mkdir "%destination%"

xcopy "%source%\*.dll" "%destination%" /s /y
xcopy "%source%\*.exe" "%destination%" /s /y


xcopy "%~dp0assets" "%destination%\assets" /s /y /e /i
xcopy "%~dp0shaders" "%destination%\shaders" /s /y /e /i

echo Done.
pause