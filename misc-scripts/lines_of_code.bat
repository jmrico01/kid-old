:: This batch file reads cpp and h files recursively and counts number of lines

@echo off

set BAT_PATH=%~dp0
set BAT_PATH=%BAT_PATH:~0,-1%
set SRC_PATH=%BAT_PATH%\..\src

setlocal
echo off
set /a totalNumLines = 0
SETLOCAL ENABLEDELAYEDEXPANSION

pushd %SRC_PATH%
for /r %%f in (*.cpp *.h ) do (
for /f %%C in ('Find /V /C "" ^< %%f') do set Count=%%C
echo !Count! lines in %%f
set /a totalNumLines+=!Count!
)
echo.
echo ==== Total: %totalNumLines% lines of code
popd
