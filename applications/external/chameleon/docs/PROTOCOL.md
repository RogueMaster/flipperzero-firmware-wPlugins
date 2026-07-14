# Chameleon Ultra Protocol Documentation

## Protocol Overview

The Chameleon Ultra uses a binary protocol over serial communication (USB/UART) and Bluetooth BLE.

## Frame Structure

Every command and response follows this structure:

```
┌─────┬──────┬──────┬────────┬─────┬──────┬──────────┬──────┐
│ SOF │ LRC1 │ CMD  │ STATUS │ LEN │ LRC2 │   DATA   │ LRC3 │
├─────┼──────┼──────┼────────┼─────┼──────┼──────────┼──────┤
│ 1B  │  1B  │  2B  │   2B   │ 2B  │  1B  │ 0-512B   │  1B  │
└─────┴──────┴──────┴────────┴─────┴──────┴──────────┴──────┘

Total: 10 + LEN bytes (10-522 bytes)
```

### Field Descriptions

- **SOF** (Start of Frame): Always `0x11`
- **LRC1**: Always `0xEF` (checksum of SOF)
- **CMD**: Command ID (Big Endian, 2 bytes)
- **STATUS**: Response status (Big Endian, 2 bytes)
  - Client → Device: Always `0x0000`
  - Device → Client: Status code
- **LEN**: Data payload length (Big Endian, 2 bytes, max 512)
- **LRC2**: Checksum of CMD|STATUS|LEN (1 byte)
- **DATA**: Command/response payload (variable, 0-512 bytes)
- **LRC3**: Checksum of DATA (1 byte)

### Checksum Calculation (LRC)

LRC (Longitudinal Redundancy Check) is calculated as:
```
LRC = ~(sum of all bytes) + 1
    = two's complement of sum modulo 256
```

Example in C:
```c
uint8_t calculate_lrc(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(~sum + 1);
}
```

## Command Categories

### Device Management (1000-1037)

| CMD  | Name                     | Description                    |
|------|--------------------------|--------------------------------|
| 1000 | GET_APP_VERSION          | Get firmware version           |
| 1001 | CHANGE_DEVICE_MODE       | Switch Reader/Emulator mode    |
| 1002 | GET_DEVICE_MODE          | Query current mode             |
| 1003 | SET_ACTIVE_SLOT          | Set active slot (0-7)          |
| 1004 | SET_SLOT_TAG_TYPE        | Configure slot tag type        |
| 1006 | SET_SLOT_ENABLE          | Enable/disable HF or LF        |
| 1007 | SET_SLOT_TAG_NICK        | Set slot nickname (UTF-8)      |
| 1011 | GET_DEVICE_CHIP_ID       | Read nRF52840 chip ID          |
| 1017 | GET_GIT_VERSION          | Get git version string         |
| 1019 | GET_SLOT_INFO            | Get all slots configuration    |
| 1024 | DELETE_SLOT_SENSE_TYPE   | Clear slot configuration       |
| 1033 | GET_DEVICE_MODEL         | Get model (Ultra=0, Lite=1)    |
| 1035 | GET_DEVICE_CAPABILITIES  | List supported commands        |

### HF Operations (2000-2012)

| CMD  | Name                     | Description                    |
|------|--------------------------|--------------------------------|
| 2000 | HF14A_SCAN               | Scan for ISO14443A tags        |
| 2001 | MF1_DETECT_SUPPORT       | Check Mifare Classic support   |
| 2007 | MF1_AUTH_ONE_KEY_BLOCK   | Authenticate sector with key   |
| 2008 | MF1_READ_ONE_BLOCK       | Read 16-byte block             |
| 2009 | MF1_WRITE_ONE_BLOCK      | Write 16-byte block            |
| 2012 | MF1_CHECK_KEYS_OF_SECTORS| Bulk key verification          |

### LF Operations (3000-3003)

| CMD  | Name                     | Description                    |
|------|--------------------------|--------------------------------|
| 3000 | EM410X_SCAN              | Read EM4100 tag                |
| 3002 | HIDPROX_SCAN             | Read HID Prox card             |

### Emulator Configuration (4000-4030)

| CMD  | Name                     | Description                    |
|------|--------------------------|--------------------------------|
| 4000 | MF1_WRITE_EMU_BLOCK_DATA | Load Mifare Classic data       |
| 4009 | MF1_GET_EMULATOR_CONFIG  | Get emulation settings         |

### LF Emulator (5000-5003)

| CMD  | Name                     | Description                    |
|------|--------------------------|--------------------------------|
| 5000 | EM410X_SET_EMU_ID        | Configure EM4100 emulation     |
| 5002 | HIDPROX_SET_EMU_ID       | Configure HID Prox emulation   |

## Status Codes

| Code   | Name                 | Description                    |
|--------|----------------------|--------------------------------|
| 0x0000 | STATUS_SUCCESS       | Command succeeded              |
| 0x0001 | STATUS_INVALID_CMD   | Unknown command                |
| 0x0002 | STATUS_INVALID_PARAM | Invalid parameters             |
| 0x0200 | STATUS_HF_TAG_OK     | HF operation succeeded         |
| 0x0201 | STATUS_HF_TAG_NO     | No HF tag detected             |
| 0x0206 | STATUS_MF_ERR_AUTH   | Mifare authentication failed   |
| 0x0300 | STATUS_LF_TAG_OK     | LF operation succeeded         |
| 0x0401 | STATUS_FLASH_READ_FAIL  | Flash read error            |
| 0x0402 | STATUS_FLASH_WRITE_FAIL | Flash write error           |

## Example Commands

### Get App Version (CMD 1000)

**Request:**
```
11 EF 03 E8 00 00 00 00 18 00
│  │  └─┬─┘ └─┬─┘ └─┬─┘ │  │
│  │    │     │     │    │  └─ LRC3 (no data)
│  │    │     │     │    └──── LRC2
│  │    │     │     └───────── LEN = 0
│  │    │     └─────────────── STATUS = 0x0000
│  │    └───────────────────── CMD = 1000 (0x03E8)
│  └────────────────────────── LRC1 = 0xEF
└───────────────────────────── SOF = 0x11
```

**Response:**
```
11 EF 03 E8 00 00 00 02 16 01 00 FE
│  │  └─┬─┘ └─┬─┘ └─┬─┘ │  └┬┘ │
│  │    │     │     │    │   │  └─ LRC3
│  │    │     │     │    │   └──── DATA (2 bytes: major.minor)
│  │    │     │     │    └──────── LRC2
│  │    │     │     └───────────── LEN = 2
│  │    │     └─────────────────── STATUS = 0x0000 (success)
│  │    └───────────────────────── CMD = 1000
│  └────────────────────────────── LRC1
└───────────────────────────────── SOF
```

### Set Active Slot (CMD 1003)

**Request to set slot 3:**
```
11 EF 03 EB 00 00 00 01 15 03 FC
│  │  └─┬─┘ └─┬─┘ └─┬─┘ │  │  │
│  │    │     │     │    │  │  └─ LRC3
│  │    │     │     │    │  └──── DATA = 0x03 (slot 3)
│  │    │     │     │    └──────── LRC2
│  │    │     │     └───────────── LEN = 1
│  │    │     └─────────────────── STATUS = 0x0000
│  │    └───────────────────────── CMD = 1003 (0x03EB)
│  └────────────────────────────── LRC1
└───────────────────────────────── SOF
```

### Set Slot Nickname (CMD 1007)

**Request to name slot 0 "MyCard":**
```
11 EF 03 EF 00 00 00 07 11 00 4D 79 43 61 72 64 8B
│  │  └─┬─┘ └─┬─┘ └─┬─┘ │  │  └──────┬──────┘ │
│  │    │     │     │    │  │         │        └─ LRC3
│  │    │     │     │    │  │         └────────── "MyCard"
│  │    │     │     │    │  └───────────────────── Slot 0
│  │    │     │     │    └──────────────────────── LRC2
│  │    │     │     └───────────────────────────── LEN = 7 (1 + 6)
│  │    │     └─────────────────────────────────── STATUS = 0x0000
│  │    └───────────────────────────────────────── CMD = 1007
│  └────────────────────────────────────────────── LRC1
└───────────────────────────────────────────────── SOF
```

## Implementation Notes

### Byte Order
- All multi-byte values use **Big Endian** (network byte order)

### String Encoding
- UTF-8 encoding, no null terminator
- Maximum 32 bytes for slot nicknames

### Key Formats
- Mifare keys: 6 bytes
- Key type: `0x60` (Key A) or `0x61` (Key B)

### Best Practices
1. Always validate SOF and LRC1 before parsing
2. Check LRC2 before processing header
3. Verify LRC3 after receiving data
4. Check STATUS code before using response data
5. Respect maximum DATA length (512 bytes)

## References

- Official protocol: [RfidResearchGroup/ChameleonUltra](https://github.com/RfidResearchGroup/ChameleonUltra)
- Protocol wiki: [ChameleonUltra/wiki/protocol](https://github.com/RfidResearchGroup/ChameleonUltra/wiki/protocol)
