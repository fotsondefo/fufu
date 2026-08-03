@echo off
call "H:\0_Will\VisualStudio\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
"H:\0_Will\VisualStudio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S "H:\0_Will\Fufu" -B "H:\0_Will\Fufu\build\debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
