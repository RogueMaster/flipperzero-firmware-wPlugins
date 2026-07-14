@echo off
REM Restore Marauder/WiFi Firmware Script for Windows
REM Flashes WiFi Marauder or other firmware back to ESP32
REM
REM Usage: restore_marauder.bat [port] [firmware.bin]

setlocal

set PORT=%1
set FIRMWARE=%2

echo.
echo ========================================
echo   Restore Marauder/WiFi Firmware
echo ========================================
echo.

REM Check if esptool is installed
where esptool.py >nul 2>nul
if errorlevel 1 (
    echo [WARN] esptool not found. Installing...
    pip install esptool
    if errorlevel 1 (
        echo [ERROR] Failed to install esptool
        echo Please install manually: pip install esptool
        goto :error
    )
)

echo [OK] esptool found

REM Check firmware file
if "%FIRMWARE%"=="" (
    echo [ERROR] No firmware file specified!
    echo.
    echo Usage: restore_marauder.bat [port] [firmware.bin]
    echo.
    echo Example: restore_marauder.bat COM3 esp32_marauder.bin
    echo.
    echo Download WiFi Marauder firmware from:
    echo   https://github.com/justcallmekoko/ESP32Marauder/releases
    echo.
    goto :error
)

if not exist "%FIRMWARE%" (
    echo [ERROR] Firmware file not found: %FIRMWARE%
    goto :error
)

echo [OK] Firmware file: %FIRMWARE%

REM Auto-detect port if not specified
if "%PORT%"=="" (
    echo [WARN] No port specified, attempting auto-detection...
    
    for %%p in (COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10) do (
        mode %%p >nul 2>nul
        if not errorlevel 1 (
            set PORT=%%p
            goto :port_found
        )
    )
    
    echo [ERROR] Could not auto-detect port.
    echo Please specify COM port as first argument.
    echo Example: restore_marauder.bat COM3 firmware.bin
    goto :error
)

:port_found
echo [OK] Using port: %PORT%

REM Display configuration
echo.
echo Configuration:
echo   Port:     %PORT%
echo   Firmware: %FIRMWARE%
echo.

REM Safety check
echo WARNING: This will erase BT Audio firmware and flash new firmware!
echo.
echo Press Ctrl+C to cancel, or any key to continue...
pause >nul

REM Detect chip type (default to esp32 if detection fails)
echo.
echo Detecting chip type...
set CHIP_TYPE=esp32
for /f "tokens=*" %%a in ('esptool.py --port %PORT% chip_id 2^>^&1 ^| findstr "Detecting chip type"') do (
    for %%b in (%%a) do set CHIP_TYPE=%%b
)
echo [OK] Using chip: %CHIP_TYPE%

REM Erase flash
echo.
echo Erasing ESP32 flash...
esptool.py --chip %CHIP_TYPE% --port %PORT% erase_flash
if errorlevel 1 (
    echo [ERROR] Erase failed!
    goto :error
)
echo [OK] Erase successful

REM Flash firmware
echo.
echo Flashing firmware...
esptool.py --chip %CHIP_TYPE% --port %PORT% --baud 921600 write_flash -z 0x1000 "%FIRMWARE%"
if errorlevel 1 (
    echo [ERROR] Flash failed!
    goto :error
)
echo [OK] Flash successful

echo.
echo ========================================
echo [SUCCESS] Firmware restore complete!
echo ========================================
echo.
echo Your ESP32 has been restored with the provided firmware.
echo.
echo To flash BT Audio again, run:
echo   flash_bt_audio.bat
echo.

goto :end

:error
echo.
echo [ERROR] Restore failed. Please check the error messages above.
exit /b 1

:end
endlocal
