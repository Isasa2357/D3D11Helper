@echo off
setlocal EnableExtensions

set "COMMIT_MESSAGE=Add CpuImage row pitch utility tests"

echo [INFO] Running build and tests...
call build_and_test.cmd
if errorlevel 1 (
    echo [ERROR] Build or tests failed. Commit and push are skipped.
    exit /b 1
)

echo [INFO] Build and tests passed. Staging v1.2.0 step2 files...
git add Test/CMakeLists.txt ^
        Test/test_cpu_image.cpp ^
        build_test_and_push_if_passed.cmd
if errorlevel 1 (
    echo [ERROR] git add failed.
    exit /b 1
)

git commit -m "%COMMIT_MESSAGE%"
if errorlevel 1 (
    echo [ERROR] git commit failed. Check git status.
    exit /b 1
)

git push
if errorlevel 1 (
    echo [ERROR] git push failed.
    exit /b 1
)

echo [OK] Build, tests, commit, and push completed.
exit /b 0
