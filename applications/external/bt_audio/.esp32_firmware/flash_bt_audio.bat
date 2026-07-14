@echo off
REM BT Audio ESP32 Flasher Script for Windows
REM Flashes BT Audio firmware to ESP32 connected to your computer
REM
REM Usage: flash_bt_audio.bat [port] [mode]
REM   port - COM port (e.g., COM3, COM4) - optional, will try to detect
REM   mode - 'source' (default) or 'sink' - optional

setlocal enabledelayedexpansion

REM Default values
set PORT=%1
set MODE=%2
set BOARD=esp32dev

if "%MODE%"=="" set MODE=source

REM Validate mode
if not "%MODE%"=="source" if not "%MODE%"=="sink" (
    echo Error: Invalid mode '%MODE%'. Must be 'source' or 'sink'.
    goto :show_help
)

REM Determine environment
if "%MODE%"=="sink" (
    set ENV=%BOARD%_sink
) else (
    set ENV=%BOARD%
)

echo.
echo ========================================
echo   BT Audio ESP32 Flasher for Flipper
echo ========================================
echo.

REM Check if PlatformIO is installed
where pio >nul 2>nul
if errorlevel 1 (
    echo Error: PlatformIO not found!
    echo.
    echo Please install PlatformIO Core:
    echo   pip install platformio
    echo.
    echo Or visit: https://platformio.org/install/cli
    goto :error
)

echo [OK] PlatformIO found

REM Auto-detect port if not specified
if "%PORT%"=="" (
    echo [WARN] No port specified, attempting auto-detection...
    
    REM Try common COM ports
    for %%p in (COM3 COM4 COM5 COM6 COM7 COM8 COM9 COM10) do (
        mode %%p >nul 2>nul
        if not errorlevel 1 (
            set PORT=%%p
            goto :port_found
        )
    )
    
    echo Error: Could not auto-detect ESP32 COM port.
    echo.
    echo Please specify port manually as first argument.
    echo Example: flash_bt_audio.bat COM3
    echo.
    echo Check Device Manager ^> Ports ^(COM ^& LPT^) for your ESP32 port.
    goto :error
)

:port_found
echo [OK] Using port: %PORT%

REM Display configuration
echo.
echo Configuration:
echo   Port:  %PORT%
echo   Mode:  %MODE%
echo   Board: %BOARD%
echo   Env:   %ENV%
echo.

REM Safety warning
echo WARNING: This will erase and flash your ESP32!
echo.
echo This will:
echo   1. Erase current ESP32 firmware
echo   2. Flash BT Audio firmware in %MODE% mode
echo   3. ESP32 will be ready for Flipper Zero BT Audio app
echo.
echo Press Ctrl+C to cancel, or any key to continue...
pause >nul

REM Build firmware
echo.
echo Building firmware...
pio run -e %ENV%
if errorlevel 1 (
    echo [ERROR] Build failed!
    goto :error
)
echo [OK] Build successful

REM Flash firmware
echo.
echo Flashing to %PORT%...
pio run -e %ENV% -t upload --upload-port %PORT%
if errorlevel 1 (
    echo [ERROR] Flash failed!
    echo.
    echo Troubleshooting:
    echo   1. Check ESP32 is connected and powered
    echo   2. Install USB drivers for your ESP32 board
    echo   3. Try holding BOOT button while connecting
    echo   4. Check Device Manager for correct COM port
    goto :error
)
echo [OK] Flash successful

REM Monitor serial output
echo.
echo Opening serial monitor...
echo (Press Ctrl+C to exit monitor)
echo.
timeout /t 2 >nul

pio device monitor --port %PORT% --baud 115200

echo.
echo ========================================
echo [SUCCESS] BT Audio firmware flash complete!
echo ========================================
echo.
echo Next steps:
echo   1. Disconnect ESP32 from computer (or flipper)
echo   2. (re)Connect ESP32 to Flipper Zero:
echo      - Flipper TX (pin 13) -^> ESP32 RX
echo      - Flipper RX (pin 14) -^> ESP32 TX
echo      - Flipper GND -^> ESP32 GND
echo   3. Launch BT Audio app on Flipper Zero
echo   4. Scan and connect to Bluetooth headphones
echo   5. Play test tone
echo.


if "%MODE%"=="source" (
    echo Mode: A2DP Source - Audio streams TO headphones/speakers
) else (
    echo Mode: A2DP Sink - Receives audio FROM phone/PC
)
echo.

goto :end

:show_help
echo.
echo BT Audio ESP32 Flasher for Windows
echo.
echo Usage: flash_bt_audio.bat [port] [mode]
echo.
echo Arguments:
echo   port - COM port (e.g., COM3, COM4)
echo          If not specified, will try to auto-detect
echo   mode - Firmware mode: 'source' (default) or 'sink'
echo          source: Stream audio TO headphones
echo          sink:   Receive audio FROM phone/PC
echo.
echo Examples:
echo   flash_bt_audio.bat              Auto-detect port, source mode
echo   flash_bt_audio.bat COM3         Use COM3, source mode
echo   flash_bt_audio.bat COM3 sink    Use COM3, sink mode
echo.
echo Requirements:
echo   - PlatformIO Core (install: pip install platformio)
echo   - ESP32 connected via USB
echo   - Drivers for ESP32 USB-to-Serial chip (CP2102, CH340, etc.)
echo.
goto :end

:error
echo.
echo [ERROR] Flash failed. Please check the error messages above.
exit /b 1

:end
endlocal
