@echo off
REM Usage: configure.bat [debug|release]
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

if /I "%BUILD_TYPE%"=="debug"   set CMAKE_BUILD_TYPE=Debug
if /I "%BUILD_TYPE%"=="release" set CMAKE_BUILD_TYPE=Release

set BUILD_DIR=build\%BUILD_TYPE%
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

cmake -S . -B %BUILD_DIR% ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

echo.
echo Configure dans %BUILD_DIR%. Pour compiler :
echo   cmake --build %BUILD_DIR%