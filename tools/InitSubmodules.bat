@echo off
setlocal

rem Resolve repository root from this script's location.
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "REPO_ROOT=%%~fI"

pushd "%REPO_ROOT%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to move to repository root: "%REPO_ROOT%"
    exit /b 1
)

echo [INFO] Repository root: "%REPO_ROOT%"
echo [INFO] Syncing submodule URLs recursively...
git submodule sync --recursive
if errorlevel 1 goto :error

echo [INFO] Initializing and updating submodules recursively...
git submodule update --init --recursive --jobs 8
if errorlevel 1 goto :error

echo [INFO] Final submodule status:
git submodule status --recursive
if errorlevel 1 goto :error

echo [OK] All submodules are initialized and updated recursively.
popd >nul
exit /b 0

:error
echo [ERROR] Submodule initialization failed.
popd >nul
exit /b 1
