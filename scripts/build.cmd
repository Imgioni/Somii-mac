@echo off
rem Builds the project with MSVC + Ninja (Release). Extra arguments go to "cmake --build",
rem e.g.  scripts\build.cmd --target SGTests
setlocal
set "PATH=%PATH%;C:\Program Files (x86)\Microsoft Visual Studio\Installer;C:\Program Files\CMake\bin;%LOCALAPPDATA%\Microsoft\WinGet\Links"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0.."
if not exist build\CMakeCache.txt (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
)
cmake --build build %* || exit /b 1
