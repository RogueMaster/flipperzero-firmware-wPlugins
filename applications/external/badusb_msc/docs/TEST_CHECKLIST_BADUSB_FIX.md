# BadUSB MSC - hardware regression checklist

This update has been reviewed statically only; **a compiled .fap and device tests are required**.

1. Back up `SD Card/apps_data/badusb_msc/drive.img`. Verify it is 8,388,608 bytes. Restart and ensure it remains intact.
2. Test script `HID_SMOKE_TEST.txt` on Windows Notepad (simple ASCII).
3. For Canadian French (Legacy) Windows, check that `fr-CA-Legacy.kl` is already available in BadUSB Config without manual copying, then choose it and run `KEYBOARD_LAYOUT_TEST_LEGACY.txt`. Compare every special character. Otherwise use the correct matching stock layout.
4. Test the provided Windows->Flipper and Flipper->Windows transfer scripts. They must use `BADUSBMSC`, not `BITOSTOR`.
5. Check BadUSB Config: USB manufacturer/product/VID/PID. Eject the volume and reopen the script to force reenumeration; check USB device descriptor on PC.
6. Test first-line `ID 1234:abcd Example:Keyboard` on a harmless test script. The composite device should advertise the requested identifier and the disk should still appear. Restore default identity afterward.
7. Test USB <-> BLE switching with a paired authorized host while the MSC volume remains mounted over USB. Ensure scripts run on the intended HID transport only.
8. Test pause/resume, stop, key release, mouse-button release, disconnect/reconnect, and opening Config repeatedly.
9. Verify no freeze, no changes to existing files, and no unintended `drive.img` recreation after app restart or firmware update.

Do not publish as a verified release until these tests pass.
