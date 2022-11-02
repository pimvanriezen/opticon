@echo off

set buildMode=dev
if NOT [%1]==[] (set buildMode=%1)

@echo on

call build-versionSourceFile.bat

cmake -DBUILD_MODE=%buildMode% -S . -B ../../build/opticon-agent/cmake -G "MinGW Makefiles"
cmake --build ../../build/opticon-agent/cmake -j 3