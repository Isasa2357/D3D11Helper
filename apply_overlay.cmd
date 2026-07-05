@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "DEST_DIR=%CD%"

if not exist "%DEST_DIR%\CMakeLists.txt" (
  echo [ERROR] Run this script from the D3D11Helper repository root.
  echo         Current directory: %DEST_DIR%
  exit /b 1
)

copy /Y "%SCRIPT_DIR%\.gitignore" "%DEST_DIR%\.gitignore" >nul
if errorlevel 1 (
  echo [ERROR] Failed to copy .gitignore.
  exit /b 1
)

echo [OK] Installed .gitignore to: %DEST_DIR%\.gitignore
endlocal
