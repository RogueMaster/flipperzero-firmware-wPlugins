# GS1 RFID Parser

This is a GS1 UHF payload parser based on version 2.2 of the [Tag Data Standard](./docs/GS1_EPC.pdf).

This currently only supports parsing SGTIN formatted EPCs.
Supporting other EPC formats is possible, but not currently planned.

Parsing User Memory is not done due to the specifications poor explanation of how to decode/encode user memory.
With the addition of the AIDC Data Toggle bit, EPCs are now able to store the additional AIs that user memory was used for.

## Useful Tools

[EPC Decoder/Encoder](https://www.gs1.org/services/epc-encoderdecoder)
[User Memory Decoder](https://www.gs1.org/services/user-memory-decoder)

# UHF Modules

The following modules are supported:
* [U107](https://www.mouser.com/ProductDetail/M5Stack/U107?qs=QNEnbhJQKva%2FtSMVYqJLlg%3D%3D)
* [YRM100](https://www.aliexpress.us/item/3256805110198094.html)

Note: This is tested using the U107.

The code for communicating with the modules is based on https://github.com/frux-c/uhf_rfid.

## Setup

Connect Cables following table below

| UHF U107 | Flipper     |
| -------- | ----------- |
| TX       | RX (Pin 14) |
| RX       | TX (Pin 13) |
| 5V       | 5V (Pin  1) |
| GND      | GND (Pin 8) |

## Useful Links

[U107 Datasheet](https://www.mouser.com/datasheet/2/1117/M5Stack_08192021_U107-2525299.pdf)

[Arduino Sample Program](https://github.com/m5stack/M5-ProductExampleCodes/tree/master/Unit/UHF_RFID)

[Firmware Commumication Protocol](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/unit/uhf_rfid/MagicRF%20M100%26QM100_Firmware_manual_en.pdf)

Additional information can be found in the [documents folder](./docs).
