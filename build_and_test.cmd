@echo off
setlocal

rmdir /s /q out\build\default 2>nul

cmake -S . -B out/build/default -G "Visual Studio 17 2022" -A x64 ^
  -DD3D11HELPER_BUILD_SAMPLES=ON ^
  -DD3D11HELPER_BUILD_TESTS=ON

if errorlevel 1 exit /b 1

cmake --build out/build/default --config Debug --parallel

if errorlevel 1 exit /b 1

ctest --test-dir out/build/default -C Debug --output-on-failure -j %NUMBER_OF_PROCESSORS%

if errorlevel 1 exit /b 1

exit /b 0
