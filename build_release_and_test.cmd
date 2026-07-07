@echo off
setlocal

echo [release] Configure Release build...
cmake -S . -B out/build/release -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

echo [release] Build Release...
cmake --build out/build/release --config Release --parallel
if errorlevel 1 exit /b 1

echo [release] Run Release tests...
ctest --test-dir out/build/release -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo [release] Release build and tests passed.
