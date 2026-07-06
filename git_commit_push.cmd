@echo off
setlocal

git add .
git commit -m "Add D3D11Helper v1.1.0 module entry points"
if errorlevel 1 exit /b 1

git push
if errorlevel 1 exit /b 1

endlocal
