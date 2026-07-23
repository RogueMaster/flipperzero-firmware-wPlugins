# XboxOne-EXXX-err-Codes
Documentation of Xbox One EXXX (E201, E105, etc...) error codes and their meaning, plus possible solutions

## Contribute
[Click here to contribute new error codes information](https://github.com/TorusHyperV/XboxOne-E-err-Codes/issues/new)

## Preamble
Xbox One shows two types of critical error codes when the console cannot boot. This list aims to become a comprehensive documentation of these codes, and advice or tutorials explaining how to repair consoles with those errors. 
* **NOTE 1:** Some of these codes might be shared with Series S/X but the meanings might be slightly different! For example, Series S/X uses SSD, not HDD. Please always double check.
* **NOTE 2:** Some official documentation exists (https://support.xbox.com/en-US/help/errors/error-code-e105) but usually the explanations are just vague or completely inaccurate. 

## Error Screen types
There are only 2 types of error screens that show EXXX codes.

<p align="center">
Screen Type 1:
</p>

If you see this screen type, it means that the Operating System has, at least, been able to start. But then it found some problems.
The Operating System can have started just from eMMC NAND memory (in the case of Xbox One) or from the SSD's hidden partition (in the case of Xbox Series)
The Operating System might have also loaded the full Operating System files from the HDD/SSD before failing.
![image](https://github.com/TorusHyperV/XboxOne-E-err-Codes/assets/100166926/5d67c9db-cf28-42af-95d9-0c7ad63fff18)

---

<p align="center">
Screen Type 2:
</p>

If you see this screen you are screwed. This is a bootloader screen, which means the OS didn't load. This means the bootloader was not able to read the basic OS files from the eMMC memory (in the case of Xbox One) or from the SSD's hidden partition (Xbox Series), or they were corrupted or some file missing.
![image](https://github.com/TorusHyperV/XboxOne-E-err-Codes/assets/100166926/b4b2684b-9c0f-4df0-b4d3-9fdb81fa34fb)

## E100 Codes
These are error codes that happen earlier in the boot process or in the Xbox's HostOS.

### E100
_Update Failed / DVD drive not paired_
This error typically occurs when a paired DVD drive is not present, and will prevent the ongoing system update from finishing.

**Solution:** Repair the original DVD drive that came with the console.

### E101:
Related to the update process - several different possible causes
**Solution:** Perform an [OSU](https://support.xbox.com/en-US/help/hardware-network/console/offline-system-update) and check that the disk is not damaged. Replace if damaged.

### E102:
Related to the update process - several different possible causes
**Solution:** Perform an [OSU](https://support.xbox.com/en-US/help/hardware-network/console/offline-system-update) and check that the disk is not damaged. Replace if damaged.

### E103:
Related to the update process - several different possible causes
**Solution:** Perform an [OSU](https://support.xbox.com/en-US/help/hardware-network/console/offline-system-update) and check that the disk is not damaged. Replace if damaged.

### E104:
Related to the update process - several different possible causes
**Solution:** Perform an [OSU](https://support.xbox.com/en-US/help/hardware-network/console/offline-system-update) and check that the disk is not damaged. Replace if damaged.

### E105:
_No HDD was found_ / _Corrupted file in the HDD_ / _Corrupted file in the eMMC_
This error seems to occur whenever an OS file cannot be found or is corrupted, either in the eMMC, or in the Hard Disk.

* **Solution 1:** Plug an HDD into the sata port. Check that the sata port is not damaged.
* **Solution 2:** eMMC might be damaged and failing to read properly. Try to dump eMMC, and then replace eMMC with a new part. Restore dump into the new eMMC. [VIDEO](https://www.youtube.com/watch?v=XJVitvPhe_M)
* **Note:** The original eMMC must be readable enough to get the console's certificate. The rest of the memory is not necessary, but an Xbox One won't work without its factory-issued unique console certificate.

### E106:
_No HDD was found_ / _Corrupted file in the HDD_ / _Corrupted file in the eMMC_

This error seems to occur whenever an OS file cannot be found or is corrupted, either in the eMMC, or in the Hard Disk. (Almost like _E105_)
**Solution:** Same as _E105_

### E107:
Unknown/Pending Documentation

### E108:
Unknown/Pending Documentation

### E109:
Invalid harddisk (Xbox One) or invalid SSD (Series S/X).

**Solution:** This error appears when swapping the hard disk (HDD) or solid state disk (SSD) of the consoles. It is unknown why some drives are invalid even after copying the files from the original drive. It might be a speed/bandwith issue (in other words: the console detecting that the disk is not fast enough) or maybe the Xbox's only accept disks from specific manufacturers (specially Series S/X and the SSDs, Xbox One seems to accept almost any hard disk (has someone tried to put like a 100Gb old HDD or something? It might trigger this error code.). **Replace the disk with another one, preferably from the same brands that Microsoft uses, or even a disk from another donor Xbox**

### E110:
Unknown/Pending Documentation

---

## E200 Codes

### E200:
Generic catch-all error code.
**Solution:** Unknown.

### E201:
Unknown/Pending Documentation

### E202:
Unknown/Pending Documentation

### E203:
The last update was interrupted.

**Solution:** Perform an [OSU](https://support.xbox.com/en-US/help/hardware-network/console/offline-system-update)

### E204:
Unknown/Pending Documentation

### E205:
An ongoing system update failed because of software reasons

**Solution:** Reboot the console and try again. If the error persists attempt an [OSU](https://support.xbox.com/en-US/help/hardware-network/console/offline-system-update). If other error codes appear, try the solutions for those instead.

### E206:
Unknown/Pending Documentation

### E207:
Unknown/Pending Documentation

### E208: 
Unknown/Pending Documentation

### E209:
Software error? Seems to happen sometimes whenever a factory reset occurs.

**Solution:** Reboot the console and try again.

### E210: 
Unknown/Pending Documentation

---

## E300 Codes
Apparently, [E3XX codes exist](https://answers.microsoft.com/en-us/xbox/forum/all/error-code-e302/a0750089-2c16-402a-8101-1feab59671ad). They seem to be rarer to come accross. Their meaning is partially unknown


## Miscellaneous / Other information
Presumably, when rogue Xbox One Internal devkits are banned, they present an irreversible `E20X` error code. (Their certificate might be wipped, according to @TitleOS)
