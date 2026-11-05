@echo off
rem SMT project command-line environment.
rem Visual Studio 2026 uses the registered Qt version and does not need this script.
rem This script is for a terminal in this folder, so qmake is Qt 5.14.2 MSVC
rem instead of the Qt 6.8.3 MinGW qmake currently first on PATH.
rem MSVC 14.51 (v145) cannot compile Qt 5.14.2 headers (stdext checked iterators removed), so 14.44 is pinned.

call "D:\VS\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.44
if errorlevel 1 (
  echo Failed to load the VS 2026 x64 build environment.
  exit /b 1
)

set "QTDIR=D:\Qt\5.14.2\msvc2017_64"
set "PATH=%QTDIR%\bin;%PATH%"
set "WindowsTargetPlatformVersion=10.0.26100.0"

echo.
echo QTDIR=%QTDIR%
qmake -v
echo Platform toolset: v143 ^(MSVC 14.44^)  Target: x64
echo Windows SDK: %WindowsTargetPlatformVersion%
echo.

rem Keep the window open when the script is double-clicked.
rem Scripts that call this one (package.bat) set SMTPRO_NO_PAUSE.
if defined SMTPRO_NO_PAUSE exit /b 0
echo %cmdcmdline% | find /I " /c " >nul
if not errorlevel 1 pause
exit /b 0
