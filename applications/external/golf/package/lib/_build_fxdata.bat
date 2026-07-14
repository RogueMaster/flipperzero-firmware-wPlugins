@echo off

cd /d %~dp0

rem compile fxdata
python fxdata-build.py fxdata.txt

if %errorlevel%==0 goto postbuildfx

pause
exit /b 1

:postbuildfx
