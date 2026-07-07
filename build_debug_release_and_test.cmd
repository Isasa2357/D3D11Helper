@echo off
setlocal EnableExtensions

set "BUILD_DIR=out/build/multi"
set "BUILD_SAMPLES=ON"
set "TEST_JOBS=%NUMBER_OF_PROCESSORS%"

if "%TEST_JOBS%"=="" set "TEST_JOBS=4"

if /I "%~1"=="--tests-only" set "BUILD_SAMPLES=OFF"
if /I "%~1"=="--no-samples" set "BUILD_SAMPLES=OFF"
if /I "%D3D11HELPER_BUILD_SAMPLES%"=="OFF" set "BUILD_SAMPLES=OFF"

echo [debug-release] Configure once...
echo [debug-release] Build samples: %BUILD_SAMPLES%
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DD3D11HELPER_BUILD_SAMPLES=%BUILD_SAMPLES% ^
  -DD3D11HELPER_BUILD_TESTS=ON
if errorlevel 1 exit /b 1

echo [debug-release] Build Debug...
cmake --build "%BUILD_DIR%" --config Debug --parallel
if errorlevel 1 exit /b 1

echo [debug-release] Test Debug...
ctest --test-dir "%BUILD_DIR%" -C Debug --output-on-failure -j %TEST_JOBS%
if errorlevel 1 exit /b 1

echo [debug-release] Build Release...
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

echo [debug-release] Test Release...
ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure -j %TEST_JOBS%
if errorlevel 1 exit /b 1

echo [debug-release] Debug and Release builds/tests passed.
endlocal
