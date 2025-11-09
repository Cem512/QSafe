@echo off
REM Flash QSafe firmware to ESP32
REM Usage: flash_esp32.bat [COM_PORT]
REM Example: flash_esp32.bat COM3

if "%1"=="" (
    echo Usage: flash_esp32.bat [COM_PORT]
    echo Example: flash_esp32.bat COM3
    exit /b 1
)

set PORT=%1

echo.
echo ========================================
echo  Flashing QSafe EEW Node to %PORT%
echo ========================================
echo.

REM Check if firmware exists
if not exist ".pio\build\esp32dev\firmware.bin" (
    echo ERROR: firmware.bin not found!
    echo Please build the project first in PlatformIO
    exit /b 1
)

REM Flash using esptool
python -m esptool --chip esp32 --port %PORT% --baud 921600 ^
    --before default_reset --after hard_reset write_flash -z ^
    --flash_mode dio --flash_freq 40m --flash_size 4MB ^
    0x1000 .pio\build\esp32dev\bootloader.bin ^
    0x8000 .pio\build\esp32dev\partitions.bin ^
    0x10000 .pio\build\esp32dev\firmware.bin

if %ERRORLEVEL% == 0 (
    echo.
    echo ========================================
    echo  Flashing completed successfully!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo  Flashing failed!
    echo ========================================
)

pause
