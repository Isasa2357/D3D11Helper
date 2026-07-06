@echo off
setlocal

call build_and_test.cmd
if errorlevel 1 (
    echo Build or tests failed. git push was skipped.
    exit /b 1
)

git add .
git commit -m "Add D3D11 Processing threshold visualization"
if errorlevel 1 exit /b 1

git push
exit /b %errorlevel%
