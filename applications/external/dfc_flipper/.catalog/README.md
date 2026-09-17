# DFC

Read and emulate generic MIFARE DESFire-compatible NFC credentials on Flipper Zero.

## Features

- Legacy DES, ISO 3DES, and AES authentication
- Standard, backup, value, and record files
- Human-readable .dfc and compiled .dfcb credentials
- Writable blank-card credentials that can be saved after emulation

## Credentials

Place saved credentials in SD Card/apps_data/dfc/. DFC uses the all-zero
factory DES key at the PICC level when no saved credential is selected.

## Disclaimer

DFC is an independent, third-party implementation of the MIFARE DESFire
native command protocol. It is not developed, authorized, licensed, or
endorsed by NXP Semiconductors. "DESFire" is used here solely to describe
protocol compatibility, not to claim affiliation.

MIFARE and DESFire are trademarks of NXP B.V.
