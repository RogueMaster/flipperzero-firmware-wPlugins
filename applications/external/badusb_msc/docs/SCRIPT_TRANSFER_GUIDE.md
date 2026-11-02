# BadUSB MSC — Windows transfer guide

This guide is for **authorized computers**. BadUSB MSC presents a keyboard interface and a drive labeled `BADUSBMSC`. A `.txt` file in `SD Card/badusb/` runs DuckyScript; files on the USB drive are on the virtual FAT16 disk, not next to your scripts.

## Start a PowerShell session (paste into a BadUSB `.txt` script)

```text
REM Configure Keyboard Layout in the BadUSB Config screen before starting
DELAY 2500
GUI r
DELAY 700
STRING powershell -NoProfile
ENTER
DELAY 1800
```

## Shared drive-detection line

```text
STRING $v=Get-Volume -FileSystemLabel BADUSBMSC -ErrorAction SilentlyContinue
ENTER
DELAY 700
```

Never assume the storage is always `G:`. The following examples use `$v.DriveLetter` returned by Windows and print an error if the drive is unavailable.

## Ready to paste: Windows Desktop → BADUSBMSC (copy)

First create `FLIPTEST.TXT` on your Windows Desktop, or use `EXAMPLE_TRANSFER.txt` to create it for you. Then append to the script after the shared detection line:

```text
STRING if($v -and $v.DriveLetter){Copy-Item -LiteralPath (Join-Path ([Environment]::GetFolderPath('Desktop')) 'FLIPTEST.TXT') -Destination ($v.DriveLetter+':\FLIPTEST.TXT') -ErrorAction Stop;Write-Host 'Copied to Flipper'}else{Write-Host 'BADUSBMSC not found'}
ENTER
```

## Ready to paste: Windows Desktop → BADUSBMSC (move)

**Move removes the file from the Desktop on success.** Use only with test data or when you intend to move the original.

```text
STRING if($v -and $v.DriveLetter){Move-Item -LiteralPath (Join-Path ([Environment]::GetFolderPath('Desktop')) 'FLIPTEST.TXT') -Destination ($v.DriveLetter+':\FLIPTEST.TXT') -ErrorAction Stop;Write-Host 'Moved to Flipper'}else{Write-Host 'BADUSBMSC not found'}
ENTER
```

## Ready to paste: BADUSBMSC → Windows Desktop (copy)

```text
STRING if($v -and $v.DriveLetter){Copy-Item -LiteralPath ($v.DriveLetter+':\FLIPTEST.TXT') -Destination (Join-Path ([Environment]::GetFolderPath('Desktop')) 'FLIPBACK.TXT') -ErrorAction Stop;Write-Host 'Copied to Desktop'}else{Write-Host 'BADUSBMSC not found'}
ENTER
```

## Ready to paste: create then transfer a text file

```text
STRING $desk=[Environment]::GetFolderPath('Desktop')
ENTER
STRING Set-Content -LiteralPath (Join-Path $desk 'FLIPTEST.TXT') -Value 'Hello from Windows!' -Encoding ASCII
ENTER
STRING $v=Get-Volume -FileSystemLabel BADUSBMSC -ErrorAction SilentlyContinue
ENTER
STRING if($v -and $v.DriveLetter){Move-Item -LiteralPath (Join-Path $desk 'FLIPTEST.TXT') -Destination ($v.DriveLetter+':\FLIPTEST.TXT') -ErrorAction Stop}else{Write-Host 'BADUSBMSC not found'}
ENTER
```

## PowerShell commands for manual testing (do NOT prefix these with STRING)

```powershell
$v = Get-Volume -FileSystemLabel BADUSBMSC -ErrorAction Stop
$drive = $v.DriveLetter + ':\'
Get-ChildItem $drive

# Send a file from your Desktop to BADUSBMSC, without deleting your original
Copy-Item -LiteralPath (Join-Path ([Environment]::GetFolderPath('Desktop')) 'FLIPTEST.TXT') -Destination ($drive + 'FLIPTEST.TXT') -ErrorAction Stop

# Bring it back to your Desktop
Copy-Item -LiteralPath ($drive + 'FLIPTEST.TXT') -Destination (Join-Path ([Environment]::GetFolderPath('Desktop')) 'FLIPBACK.TXT') -ErrorAction Stop
```

## When a script types the wrong characters

If characters are typed incorrectly, check that **BadUSB → Config → Keyboard Layout** matches the layout active on the target computer. Test with a simple string in a text editor before running transfer scripts.

## Important file rules

- BADUSBMSC is a separate **8 MiB FAT16 virtual drive** with a dynamic drive letter on Windows.
- For synchronization into qFlipper's `apps_data/badusb_msc/files/`, use **root-level 8.3 names** such as `FLIPTEST.TXT`. Nested folders and long names are outside the mirror's current support.
- Put `.txt` **scripts** into `SD Card/badusb/`; put actual **transfer data** in BADUSBMSC, and retrieve the synchronized copy from `SD Card/apps_data/badusb_msc/files/` only after exiting the app.
- Eject BADUSBMSC from Windows before stopping the Flipper USB session, then use qFlipper afterward. Avoid concurrent modification.
