
# ARM SWD (Single Wire Debug) Probe

Modern microcontrollers have support for the two wire debug interface SWD, which makes wiring a lot simpler.
When reverse engineering, finding these two pins is a lot easier than with JTAG, where you had to wire up twice or more pins. However, finding the two pins is still some work, which gets simplified even more with this application.

This application tries to detect a valid SWD response on the wires you have picked and beeps when you have found the correct pins, showing the detected ID register and, more important, the SWD pinout. It doesn't matter which two pins you choose, just pick any two from the GPIOs on the breakout header.

To achieve this, the application sends packets and scans the response on all pins and elaborates the pins within a few retries. Using some kind of bisect pattern reduces this number to a hand full of tries, yielding in a seemingly instant detection.

For the user it is as simple as a continuity tester – wire up your two test needles (or acupuncture needles), connect the obvious GND pin and probe all test pads.
How long it takes depends on your bisect capabilities when finding all pad combinations.

Video 1: https://www.youtube.com/watch?v=4vAWFcUPVeo  
Video 2: https://www.youtube.com/watch?v=dmkluzQWcp4

Discussion thread: https://discord.com/channels/740930220399525928/1071712925171056690

---

## Tested Targets (ID Codes)

```

0x6BA02477 (0xBC rev 6) Flipper itself
0x0BC11477 (0xBC rev 0) B-L072Z-LRWAN1 STM32L0 Discovery kit LoRa, Sigfox, low-power wireless
0x1BA01477 (0xBA rev 1) STM32WB5MM-DK Discovery kit with STM32WB5MMG MCU
0x0BB11477 (0xBB rev 0) Bluetooth gyroball - firmware suggests PanChip PAN1020
0x2BA01477 (0xBA rev 2) STM32F401 Devboard
0x0BB11477 (0xBB rev 0) iFlight 50A ESC with seemingly F405 clones
0x0BC11477 (0xBC rev 0) DA14531-based custom PCB

```

---

## Scripting Support

Scripts go into:

```

/sd/swd/<name>.swd

```

Available commands:

```

# <comment>

message <delay_ms> <string>
beep [<id>]           # 0=success, 1=fail
apscan
apselect <ap_id>
max_tries <retries>
swd_clock_delay <us>
block_size <size>     # 4–4096
abort
mem_dump <file> <start> <length> [<flags>]   # flags&1: skip failed blocks, flags&2: finish even if a block failed
mem_ldmst <address> <data> <mask>
mem_write <address> <data>

```

### Example Script

```

apselect 0
max_tries 50
block_size 1024
mem_dump /ext/swd/flash.bin 0x08000000 0x100000 2
beep 0
message 5 "Reading sucessful"

```

---


