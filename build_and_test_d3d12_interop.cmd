@echo off
setlocal EnableExtensions

set "BRANCH=feature/v1.8.2-d3d12-interop-tests"
set "BUILD_DIR=out/build/v1_8_2_d3d12_interop"
set "D3D12HELPER_REPO=https://github.com/Isasa2357/D3D12Helper.git"
set "D3D12HELPER_TAG=main"
set "TEST_JOBS=%NUMBER_OF_PROCESSORS%"

if "%TEST_JOBS%"=="" set "TEST_JOBS=4"

if /I "%~1"=="--release" set "BUILD_DIR=out/build/v1_8_2_d3d12_interop_release"
if /I "%~1"=="--debug" set "BUILD_DIR=out/build/v1_8_2_d3d12_interop"
if not "%~2"=="" set "BUILD_DIR=%~2"

echo [interop] Fetch branch...
git fetch origin
if errorlevel 1 exit /b 1

echo [interop] Switch branch %BRANCH%...
git switch %BRANCH%
if errorlevel 1 exit /b 1

echo [interop] Pull branch...
git pull
if errorlevel 1 exit /b 1

echo [interop] Configure Debug/Release build tree: %BUILD_DIR%
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DD3D11HELPER_BUILD_SAMPLES=ON ^
  -DD3D11HELPER_BUILD_TESTS=ON ^
  -DD3D11HELPER_ENABLE_D3D12_INTEROP_TESTS=ON ^
  -DD3D11HELPER_D3D12HELPER_GIT_REPOSITORY="%D3D12HELPER_REPO%" ^
  -DD3D11HELPER_D3D12HELPER_GIT_TAG="%D3D12HELPER_TAG%"
if errorlevel 1 exit /b 1

if /I "%~1"=="--release" goto RELEASE_ONLY

echo [interop] Build Debug...
cmake --build "%BUILD_DIR%" --config Debug --parallel
if errorlevel 1 exit /b 1

echo [interop] Test Debug...
ctest --test-dir "%BUILD_DIR%" -C Debug --output-on-failure --parallel %TEST_JOBS%
if errorlevel 1 exit /b 1

if /I "%~1"=="--debug" goto DONE

:RELEASE_ONLY
echo [interop] Build Release...
cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

echo [interop] Test Release...
ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure --parallel %TEST_JOBS%
if errorlevel 1 exit /b 1

:DONE
echo [interop] Build/test completed successfully.
endlocal
