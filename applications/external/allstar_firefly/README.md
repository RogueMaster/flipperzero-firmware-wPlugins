# Allstar Firefly 318ALD31K — Flipper Zero SubGHz Protocol

Decoder and encoder for the **Allstar Firefly 318ALD31K** gate remote, based on the **Supertex ED-9** trinary encoder IC. Includes both a standalone FAP for protocol development/testing and a native SubGHz protocol integration.

## Protocol Details

| Parameter | Value |
|---|---|
| Frequency | 318 MHz |
| Modulation | OOK (On-Off Keying) |
| Preset | FuriHalSubGhzPresetOok650Async |
| Encoder IC | Supertex ED-9 |
| Code type | Static 9-bit trinary (DIP switch) |
| Symbols per frame | 18 (2 per bit) |
| Sync pulse | None |
| Frame repeats | 20 per keypress |
| Inter-frame gap | ~30,440 us |

## Symbol Encoding

Each DIP switch position is encoded as two consecutive pulse+gap pairs:

| DIP State | Symbol | Pattern |
|---|---|---|
| ON (+) | H H | 4045us HIGH, 607us LOW x2 |
| OFF (-) | L L | 530us HIGH, 4139us LOW x2 |
| FLOAT (0) | H L | 4045us HIGH, 607us LOW, 530us HIGH, 4139us LOW |

Timing derived from analysis of 37 clean frames across two independent captures.

## Save File Format

```
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: 318000000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: Allstar Firefly
Key: +--000++-
```

The Key field is exactly 9 characters using +, -, or 0 per DIP switch position (left to right, switch 1-9).

## Repository Structure

```
flipper-allstar-firefly/
├── native_protocol/
│   ├── allstar_firefly.h      # Protocol header + extern declarations
│   ├── allstar_firefly.c      # Full decoder + encoder implementation
│   └── INTEGRATION.txt        # Step-by-step registration instructions
└── fap/
    ├── allstar_firefly.h      # FAP header
    ├── allstar_firefly.c      # FAP implementation (standalone app)
    └── application.fam        # FAP manifest
```

## Native Protocol Installation

1. Copy native_protocol/allstar_firefly.h and allstar_firefly.c into lib/subghz/protocols/ in your firmware tree.

2. In lib/subghz/protocols/protocol_list.h add:
```c
#include "allstar_firefly.h"
```

3. In lib/subghz/protocols/protocol_list.c add to the protocol array:
```c
&subghz_protocol_allstar_firefly,
```

4. Rebuild and flash. The protocol appears in the SubGHz app alongside CAME, Princeton, etc.

See native_protocol/INTEGRATION.txt for full details.

## FAP Installation

Copy the fap/ folder into applications_user/ in your firmware tree and build with:

```
fbt fap_allstar_firefly
```

The FAP provides LISTEN mode with live decode and RSSI bar, EDIT mode with cursor-based DIP switch editor, and TRANSMIT mode sending 20 frame repetitions.

## Validated Against

- Allstar Firefly 318ALD31K remote (Supertex ED-9 encoder IC confirmed)
- Timing verified from 37 clean frames across two independent raw captures
- TX verified by Flipper-to-Flipper loopback decode
- Native protocol tested: live decode, save/load .sub files, retransmit

## Compatible Firmware

- Unleashed (unlshd-086+)
- Stock OFW (same protocol registration API)

## License

MIT — see LICENSE
