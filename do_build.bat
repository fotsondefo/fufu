@echo off
call "H:\0_Will\VisualStudio\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
"H:\0_Will\VisualStudio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build "H:\0_Will\Fufu\build\debug" --target FufuEngine FufuStudio -- -j4
