@echo off
REM Build and Prepare QSafe Firmware for Release
REM =============================================

REM Change to script directory
cd /d "%~dp0"

echo.
echo ========================================
echo Building QSafe Firmware v1.1.0
echo ========================================
echo.
echo Working Directory: %CD%
echo.

REM Try different PlatformIO locations
set PLATFORMIO_FOUND=0

REM Method 1: Try platformio command (if in PATH)
where platformio >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Found: platformio command
    set PIO_CMD=platformio
    set PLATFORMIO_FOUND=1
    goto :build
)

REM Method 2: Try pio command (if in PATH)
where pio >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Found: pio command
    set PIO_CMD=pio
    set PLATFORMIO_FOUND=1
    goto :build
)

REM Method 3: Try VS Code PlatformIO
if exist "%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" (
    echo Found: VS Code PlatformIO
    set PIO_CMD="%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"
    set PLATFORMIO_FOUND=1
    goto :build
)

REM Method 4: Try common installation path
if exist "C:\Users\%USERNAME%\.platformio\penv\Scripts\platformio.exe" (
    echo Found: PlatformIO in user directory
    set PIO_CMD="C:\Users\%USERNAME%\.platformio\penv\Scripts\platformio.exe"
    set PLATFORMIO_FOUND=1
    goto :build
)

REM If not found, show error
if %PLATFORMIO_FOUND% EQU 0 (
    echo.
    echo ERROR: PlatformIO not found!
    echo.
    echo Please install PlatformIO:
    echo   1. Open VS Code
    echo   2. Go to Extensions
    echo   3. Install "PlatformIO IDE"
    echo.
    echo Or run from VS Code PlatformIO terminal:
    echo   platformio run --environment esp32dev
    echo.
    pause
    exit /b 1
)

:build
echo.
echo Building firmware...
echo Command: %PIO_CMD% run --environment esp32dev
echo.

%PIO_CMD% run --environment esp32dev

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed!
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build successful!
echo ========================================
echo.

REM Check if firmware.bin exists
if not exist ".pio\build\esp32dev\firmware.bin" (
    echo ERROR: firmware.bin not found!
    echo Expected location: .pio\build\esp32dev\firmware.bin
    pause
    exit /b 1
)

REM Copy and rename firmware files
echo Copying firmware files...
copy /Y ".pio\build\esp32dev\firmware.bin" "qsafe-ota.bin"
copy /Y ".pio\build\esp32dev\firmware.bin" "qsafe-esp32-1.1.0.bin"

echo.
echo ========================================
echo Firmware files ready for upload:
echo ========================================
echo.
dir /b qsafe-*.bin
echo.

REM Get file size
for %%A in (qsafe-ota.bin) do echo Size: %%~zA bytes
echo.

REM Generate MD5 hash
echo Generating MD5 checksum...
powershell -Command "Get-FileHash qsafe-ota.bin -Algorithm MD5 | Select-Object -ExpandProperty Hash" > md5.txt
set /p MD5_HASH=<md5.txt
echo MD5: %MD5_HASH%
echo.

REM Create metadata JSON
echo Creating firmware-metadata.json...
(
echo {
echo   "version": "1.1.0",
echo   "filename": "qsafe-ota.bin",
echo   "md5": "%MD5_HASH%",
echo   "build_date": "%date% %time%"
echo }
) > firmware-metadata.json

echo.
echo ========================================
echo DONE! Next steps:
echo ========================================
echo.
echo 1. Go to: https://github.com/Cem512/QSafe/releases/tag/v1.1.0
echo 2. Click "Edit" button
echo 3. Upload these files:
echo    - qsafe-ota.bin
echo    - qsafe-esp32-1.1.0.bin
echo    - firmware-metadata.json
echo 4. Click "Update release"
echo.
echo Files are in: %CD%
echo.

pause
