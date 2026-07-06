@echo off
setlocal

git add .
git commit -m "Add D3D11 Processing threshold visualization"
if errorlevel 1 exit /b 1

git push
exit /b %errorlevel%
