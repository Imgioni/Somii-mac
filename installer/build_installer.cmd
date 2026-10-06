@echo off
rem Builds the plugin, then packages it into dist\Somii-<version>-Windows-x64-Setup.exe
rem and a matching .zip for people who would rather not run an installer.
setlocal
cd /d "%~dp0.."

echo == checking the web UI parses ==
node tests\UiSyntaxTests.mjs || exit /b 1

echo == building the plugin ==
call scripts\build.cmd --target Geminus_VST3 Geminus_CLAP Geminus_Standalone || exit /b 1

set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo Inno Setup 6 not found. Install it with:  winget install JRSoftware.InnoSetup
    exit /b 1
)

echo == building the installer ==
"%ISCC%" /Qp "installer\Somii.iss" || exit /b 1

echo == building the zip ==
powershell -NoProfile -ExecutionPolicy Bypass -File "installer\make_zip.ps1" || exit /b 1

echo.
dir /b dist
