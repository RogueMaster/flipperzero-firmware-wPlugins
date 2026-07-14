# About

This tool can unseal(unlock config read/write operations) and reset(cold restart with default configuration) Flipper Zero battery gauge.
Normally you don't need to use it, all modern firmwares can correctly handle all abnormal situations. However. If you use downgrading from 1.1 you'll have to unseal and reset battery gauge.

# How to use this tool

- Use ufbt with appropriate SDK to compile and launch tool
- Press `Unseal` and wait status to change to UNSEAL
- Press `Full` and wait for status change to FULL
- Press `Reset` and wait for gauge to complete reset procedures and return into UNSEAL state
- Restart your flipper
