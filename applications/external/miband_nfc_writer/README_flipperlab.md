# Mi Band NFC Writer

NFC writer for Mi Band with 0xFF keys support and automatic verification.

## Features
- Quick UID check with disk scanning
- Magic card emulation for Mi Band preparation
- Write original data with automatic backup
- Verify written data integrity
- Save magic dumps for easy handling
- Detailed progress tracking
- Logging system with export capability

## Usage

Quick Start
1. Load NFC dump from the nfc folder
2. Emulate magic template for blank Mi Bands
3. Write data with progress tracking
4. Verify write results

Key Operations
- Quick UID Check: Read card UID and find matching files
- Magic Emulation: Create blank template with 0xFF keys
- Write Data: Write dump to Mi Band with backup option
- Verify: Compare written data with original
- Save Magic: Convert dumps to 0xFF key format

## Settings
Configure auto-backup, verification, logging, and progress display options via Settings menu.

## File Locations
- Dumps: nfc folder
- Backups: nfc/backups folder
- Logs: apps_data/miband_nfc/logs folder
- Settings: apps_data/miband_nfc/settings.bin

## Troubleshooting
- Ensure Mi Band is magic card type
- Check card positioning during operations
- Use Emulate Magic Card before first write
- Enable logging for detailed error information

## Support
Check Flipper Zero community forums for updates and support.

Version: 1.0
Category: NFC
Developer: LucasMac