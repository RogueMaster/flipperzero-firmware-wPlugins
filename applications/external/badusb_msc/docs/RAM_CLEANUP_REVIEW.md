# BadUSB MSC v0.1 — memory cleanup review

## Scope

This change is deliberately limited to resource lifecycle safety and English source comments. The existing BadUSB MSC features, keyboard mappings, and the two original US examples have not been changed. The Canadian-French test scripts are **not** included in this package.

## Fixes included

- **Previously requested by reviewer:** release the HID semaphore in `hid_deinit()` and clear its pointer; display `I_badusb_10px` in the internal file browser.
- **Storage record lifetime:** pair the `furi_record_open(RECORD_STORAGE)` in the DuckyScript worker with `furi_record_close(RECORD_STORAGE)` when the worker exits.
- **USB callback safety:** zero-initialize `MassStorageUsb` before registering its callbacks with USB. This prevents uninitialized callback fields from being read during startup or failure cleanup.
- **Bluetooth callback safety:** explicitly initialize the BLE state callback, context, and connection state before registering the Bluetooth status callback.
- **Source readability:** translate French comments to English without changing their associated code.

## Verification performed

- Compared the final source to the original uploaded ZIP file-by-file.
- Confirmed the original `EXAMPLE_TRANSFER.txt` and `EXAMPLE_RETURN.txt` were left byte-for-byte unchanged.
- Checked the affected resource-open/close pairs, callback initialization, the HID semaphore release, and the file browser icon reference statically.
- Validated the ZIP integrity and verified no precompiled `.fap` or CA-FR test scripts were packaged.

## Still required before publication

1. Build with `ufbt -j 1` against the appropriate firmware API, then install the **newly compiled** `.fap`. The source package intentionally excludes the old precompiled `.fap`.
2. Test USB HID, USB MSC read/write, BLE HID, repeated script switching, and safe USB ejection on an actual Flipper Zero.
3. Confirm the internal file browser displays the BadUSB icon rather than a question mark.
4. With BadUSB MSC completely closed, record the `free` output through the Flipper CLI. Run 30-50 open/close cycles, recording after each set, and compare values at the same idle state. Repeat without launching the app to establish a control. Watch `Free heap size` and `Maximum heap block`.
5. If memory usage continues decreasing, collect reproducible logs and check the application versus firmware/background allocations before changing more code.
6. After successful hardware tests, commit to GitHub and update the Flipper Lab catalog manifest's `commit_sha` to that exact commit.

**Status:** Source inspection only. This package has **not** been compiled with the Flipper SDK or validated on hardware in this environment. It does not establish that all possible memory leaks have been eliminated.
