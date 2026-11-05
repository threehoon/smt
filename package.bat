@echo off
rem Builds the SwiftPlace installer: dist\SwiftPlace-<version>-setup.exe
rem
rem   1. Release build (MSVC 14.44 + Qt 5.14.2, preset "release")
rem   2. cmake --install into build\release\stage (exe + windeployqt + MSVC runtime)
rem   3. Inno Setup 6 packs stage\ into the installer
rem
rem build\release\stage on its own is a portable copy: zip it to run without installing.
setlocal
cd /d "%~dp0"

set "SMTPRO_NO_PAUSE=1"
call "%~dp0dev-env.bat" || goto :fail

cmake --preset release || goto :fail
cmake --build --preset release || goto :fail
ctest --preset release || goto :fail

if exist build\release\stage rmdir /s /q build\release\stage
cmake --install build\release --prefix build\release\stage || goto :fail

set "ISCC="
if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC goto :no_iscc
"%ISCC%" /Qp "/DBuildDir=%CD%\build\release" packaging\installer.iss || goto :fail

echo.
echo Installer written to %CD%\dist
goto :end

:no_iscc
echo Inno Setup 6 not found. Install it: winget install JRSoftware.InnoSetup

:fail
echo.
echo Packaging failed.
set "FAILED=1"

:end
echo %cmdcmdline% | find /I " /c " >nul
if not errorlevel 1 pause
if defined FAILED exit /b 1
exit /b 0
