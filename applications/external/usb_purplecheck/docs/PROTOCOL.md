# Standalone observations

0.3 has no CDC protocol or desktop receiver. The only file exchange is an isolated synthetic test volume; host file operations are initiated by the operator or host software.

The keyboard test uses the firmware HID descriptor and submits only its session marker after manual OK. Host acceptance of USB configuration and successful report submission are separate from application-level reception.

The MSC tests use an isolated 2048-sector FAT12 image. Successful READ/WRITE backend calls increment block counters; no application identity or human file-open action is inferred from these calls. The read-only profile advertises SCSI write protection and refuses writes on the device. Optional SEND.TXT contains 4096 bytes of session-specific printable data. RETURN.TXT starts zero-filled. After the USB worker has stopped, the app verifies the bounded FAT12 chain and compares all returned bytes, recording a CRC32. A match proves only the synthetic return pattern on the image, not PC disk persistence. Backend SEND-sector reads can be caused by host caching or indexing.

Observations are saved separately from the operator-selected conclusion. No response is inconclusive. The last-result record is checked for magic, length, enum bounds and token format before display. Full sessions remain JSONL on SD, with local RTC and device-monotonic elapsed times. No clock synchronization or signed evidence is claimed.
