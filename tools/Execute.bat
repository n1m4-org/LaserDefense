@echo off
chcp 65001 >nul
echo Execute.bat - Building and running GameTemplate
echo.

REM Step 1: Run AssetsCopy.bat
echo [1/3] Running AssetsCopy.bat to copy assets...
call "%~dp0AssetsCopy.bat"
if %errorlevel% neq 0 (
    echo Error: AssetsCopy.bat failed
    pause
    exit /b 1
)

REM Step 1.5: Copy assets to exe location
echo [1.5/3] Copying assets to exe location...
cd /d "%~dp0.."
if exist ".\out\x64\Debug\Game\" (
    if exist ".\Assets\" (
        xcopy ".\Assets" ".\out\x64\Debug\Game\Assets\" /E /I /Y >nul
        echo Assets copied to exe location successfully!
    ) else (
        echo Warning: Assets directory not found
    )
) else (
    echo Note: Exe output directory does not exist yet, will be created after build
)

REM Step 2: Clean and rebuild GameTemplate in Debug mode
echo.
echo [2/3] Cleaning and rebuilding GameTemplate in Debug mode...
cd /d "%~dp0.."
msbuild GameTemplate.sln /t:Clean /p:Configuration=Debug /p:Platform=x64
msbuild GameTemplate.sln /p:Configuration=Debug /p:Platform=x64 /m
if %errorlevel% neq 0 (
    echo Error: Build failed
    pause
    exit /b 1
)

REM Step 2.5: Copy assets to exe location after build
echo.
echo [2.5/3] Ensuring assets are copied to exe location...
if exist ".\Assets\" (
    xcopy ".\Assets" ".\out\x64\Debug\Game\Assets\" /E /I /Y >nul
    echo Assets copied to exe location after build!
)

REM Step 3: Launch the generated exe
echo.
echo [3/3] Launching Game.exe...
if exist ".\out\x64\Debug\Game\Game.exe" (
    start "" ".\out\x64\Debug\Game\Game.exe"
    echo Game launched successfully!
) else (
    echo Error: Game.exe not found at .\out\x64\Debug\Game\Game.exe
    pause
    exit /b 1
)

pause