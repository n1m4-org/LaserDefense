@echo off
chcp 65001 >nul
echo CopyShader.bat - Copying shaders from Engine to GameTemplate
echo.

REM Run from GameTemplate directory
REM Copy from Engine/Assets/Shaders to GameTemplate/Assets/Shaders

REM Source path: Engine\Assets\Shaders
set SOURCE_PATH=..\Engine\Assets\Shaders

REM Destination path: Assets\Shaders (current directory)
set DEST_PATH=..\Assets\Shaders

REM Check if source directory exists
if not exist "%SOURCE_PATH%" (
    echo Error: Source directory "%SOURCE_PATH%" not found
    echo Please run this script from GameTemplate directory
    pause
    exit /b 1
)

echo Copying shaders from "%SOURCE_PATH%" to "%DEST_PATH%"
echo.

REM Create Shaders directory if it doesn't exist
if not exist "%DEST_PATH%" (
    echo Creating destination directory "%DEST_PATH%"
    mkdir "%DEST_PATH%"
)

REM /E: Copy subdirectories and files (including empty directories)
REM /Y: Overwrite without confirmation
REM /I: Assume destination is a directory
xcopy "%SOURCE_PATH%\*" "%DEST_PATH%\" /E /Y /I

if %errorlevel% equ 0 (
    echo.
    echo Shader copy completed successfully!
) else (
    echo.
    echo Error occurred during copy operation
    pause
    exit /b 1
)

echo.
pause
