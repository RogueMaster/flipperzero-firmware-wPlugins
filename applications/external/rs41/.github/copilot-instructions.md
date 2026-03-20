# Copilot Agent Instructions — RS41 Tracker (Flipper Zero)

## Project overview

Flipper Zero external app that decodes Vaisala RS41 radiosondes via the built-in
or external CC1101 sub-GHz radio.  Written in C, built with **ufbt** (micro Flipper
Build Tool).  Developed by OK1CHP (Libor).

Target hardware: Flipper Zero, firmware 1.4.3 release, API 87.1, Target 7.
USB port: `/dev/ttyACM1`.

## Allowed commands

```bash
# Build the app
cd rs41_tracker && ufbt build

# Build and install to Flipper (USB)
cd rs41_tracker && ufbt launch

# Open serial console on the Flipper
ufbt cli
# Then inside the console, type: log

# Check ufbt version / SDK
ufbt status
```

## File structure

```
rs41_tracker/
├── application.fam      # Flipper app manifest (name, id, entry point, category)
├── radio_hw.h           # CC1101 hardware abstraction API (RadioSource enum + 9 functions)
├── radio_hw.c           # CC1101 hardware abstraction: internal (furi_hal_subghz_*) and
│                        #   external (direct SPI on furi_hal_spi_bus_handle_external)
├── rs41_app.c           # Main app: CC1101 preset, radio thread, GUI callbacks
├── rs41_decoder.h       # Public API: Rs41Frame struct, rs41_decode() prototype
├── rs41_decoder.c       # Protocol decoder: bit-reverse, XOR descramble, TLV parse
├── rs41_rs.h            # Reed-Solomon RS(156,132) decoder API
├── rs41_rs.c            # Reed-Solomon implementation (GF(2^8), BM+Chien+Forney)
└── README.md
```

## Key technical facts

### Radio hardware abstraction (radio_hw.h / radio_hw.c)

Two backends selected by `RadioSource` enum:
- `RadioInternal` — built-in CC1101 via `furi_hal_subghz_*` + direct SPI on
  `furi_hal_spi_bus_handle_subghz`
- `RadioExternal` — external CC1101 module via direct SPI on
  `furi_hal_spi_bus_handle_external` (PA4 chip-select)

All radio calls in rs41_app.c go through `radio_hw_*` functions.
User toggles source with **LEFT** key on idle decoder screen.

### External CC1101 SPI wiring (Flipper GPIO header)
| CC1101 | Flipper Pin | STM32 | Function   |
|--------|-------------|-------|-----------|
| VCC    | 3           | —     | 3.3 V     |
| GND    | 2           | —     | GND       |
| MOSI   | 12          | PA7   | SPI1_MOSI |
| MISO   | 13          | PA6   | SPI1_MISO |
| SCK    | 15          | PB3   | SPI1_CLK  |
| CSN    | 14          | PA4   | SPI1_NSS  |
| GDO0   | 7           | PC3   | optional  |

### CC1101 custom preset (CRITICAL)
`furi_hal_subghz_load_custom_preset()` reads `{reg, val}` pairs until `reg==0x00`,
then reads **exactly 8 more bytes** as the PA table.  Omitting those 8 bytes causes
an immediate crash.

### furi_hal_subghz_read_packet bug (CRITICAL, worked around)
The Flipper firmware's `cc1101_read_fifo` silently consumes `FIFO[0]` as a
packet-length byte, losing one byte per call.  This app bypasses it with direct
SPI access via `radio_hw_read_fifo()`.

### CC1101 radio parameters for RS41
| Register | Value | Meaning |
|----------|-------|---------|
| MDMCFG4 | 0xF7  | BW ≈ 58 kHz, exponent |
| MDMCFG3 | 0x83  | 4800 bps |
| DEVIATN | 0x04  | deviation ≈ 2381 Hz |
| PKTCTRL0 | 0x02 | infinite packet mode, no whitening, no CRC |
| SYNC1/0  | 0x08/0x6D | sync word (bit-reversed preamble) |
| IOCFG0   | 0x0D  | GDO0 asserts on sync, deasserts after packet |

### RS41 frame decoding pipeline
1. Bit-reverse each FIFO byte (CC1101 MSB-first ↔ RS41 LSB-first)
2. XOR descramble with 64-byte mask (`RS41_MASK[]`), offset 8 for sync word length
3. Reed-Solomon correction (two interleaved RS(156,132) codewords)
4. Skip bytes 0–47 (RS parity) and byte 48 (frame type)
5. Parse TLV blocks from byte 49; each validated with CRC-16/CCITT-FALSE

### XOR mask
The mask in `rs41_decoder.c` is correct for CC1101 bit-reversed data with offset +8.
The "standard" solvespace mask (for SDR/LSB-first data) does **not** work here.

### TLV block structure
```
[1B block_id] [1B block_len] [block_len bytes data] [2B CRC16-CCITT-FALSE LE]
```

Key block IDs:
- `0x79` STATUS: `data[2..9]` = 8-char serial; `data[23]` = calframe index (0–50);
  `data[24..39]` = 16 bytes of calibration payload
- `0x7A` PTU: 12× uint24-LE ADC; `meas[0,1,2]` = T_main/ref1/ref2;
  `meas[3,4,5]` = P_main/ref1/ref2
- `0x7B` GPS: `data[0..11]` = ECEF x/y/z (int32 × 0.01 m);
  `data[12..17]` = ECEF velocities (int16 × 0.01 m/s); `data[19]` = numSats

### Temperature calibration (calframes 3–6)
Each STATUS frame rotates through calframe indices 0–50, carrying 16 bytes.
Calframes 3, 4, 5, 6 contain `Rf1`, `Rf2`, `co1[3]`, `calT1[3]` (float32 LE,
some spanning frame boundaries).  Formula from rs1729/rs41ptu.c:
```
g  = (f2-f1)/(Rf2-Rf1);  Rb = (f1*Rf2-f2*Rf1)/(f2-f1)
Rc = meas[0]/g - Rb;     R  = Rc*calT1[0]
T  = (co1[0] + co1[1]*R + co1[2]*R² + calT1[1]) * (1+calT1[2])
```

### Pressure calibration (calframes 6–7, RS41-SGP only)
Calframes 6 and 7 contain `Fp1`, `Fp2`, `Cp[3]`.  Same formula structure:
```
g  = (fp2-fp1)/(Fp2-Fp1);  Rb = (fp1*Fp2-fp2*Fp1)/(fp2-fp1)
Rc = meas[3]/g - Rb
P  = Cp[0] + Cp[1]*Rc + Cp[2]*Rc²     (hPa)
```

### Rs41Frame struct (rs41_decoder.h)
```c
typedef struct {
    char    id[9];    // sonde serial, NUL-terminated
    float   lat;      // degrees, N > 0
    float   lon;      // degrees, E > 0
    float   alt;      // metres MSL
    uint8_t sats;     // GPS satellites
    float   temp;     // °C; -999.0f = not yet decoded
    float   pres;     // hPa; -1.0f  = not yet decoded
    int16_t rssi;     // dBm
    bool    has_gps;  // lat/lon/alt valid
} Rs41Frame;
```

### Signal quality history (AppCtx)
Ring buffer `sig_hist[SIG_HIST_LEN=10]` in AppCtx, guarded by mutex.
`sig_head` = next write index.  `sig_any` = true once first entry written.
Pushed after every decode attempt: 1 = OK, 0 = failed.
Rendered in draw_cb as 10 × (4×4 px) boxes: filled=OK, outline=fail.

## Compiler constraints

The Flipper SDK enables `-Werror=double-promotion` and `-Werror=format-truncation`.
- Use `f`-suffix literals: `1013.25f`, `2.25577e-5f`, etc.
- Cast float args to `(double)` for `%f`/`%.1f` in `snprintf`.
- Size `snprintf` buffers conservatively so GCC can prove no truncation.

## Display layout (128×64 px, FontSecondary = 8 px)

```
RS41  405.1 MHz [RX] IN    ← FontPrimary y=8; "IN"/"EX" FontSecondary aligned right
──────────────────────     ← separator line y=10
ID:N1320638  -84dBm        ← y=20
14.6685N  037.0377W        ← y=29
Alt: 1460m T:+19.0C        ← y=38
P: 849.1hPa  Sats: 0       ← y=47
──────────────────────     ← separator y=49 (only when sig_any)
■□■■□□■■■□                 ← sig_hist 10 × 4px boxes y=51 (filled=OK, outline=fail)
OK:stop RT:scan UP/DN      ← hint y=63
```

Fields display `---` until calibration is available.

## Known limitations / future work

- GPS ECEF→LLA uses float Bowring iteration (~1 m accuracy, fine for sondes)
- Pressure only available on RS41-SGP variant (not standard RS41-SG)


## Key technical facts

### CC1101 custom preset (CRITICAL)
`furi_hal_subghz_load_custom_preset()` reads `{reg, val}` pairs until `reg==0x00`,
then reads **exactly 8 more bytes** as the PA table.  Omitting those 8 bytes causes
an immediate crash.

### furi_hal_subghz_read_packet bug (CRITICAL, worked around)
The Flipper firmware's `cc1101_read_fifo` (address `0x801d3c8`) silently consumes
`FIFO[0]` as a packet-length byte, losing one byte per call.  This app bypasses it
with direct SPI access:
- Acquire bus with `furi_hal_spi_acquire(&furi_hal_spi_bus_handle_subghz)`
- Burst-read using `furi_hal_spi_bus_trx` + `furi_hal_spi_bus_rx`
- Release with `furi_hal_spi_release`

### CC1101 radio parameters for RS41
| Register | Value | Meaning |
|----------|-------|---------|
| MDMCFG4 | 0xF7  | BW ≈ 58 kHz, exponent |
| MDMCFG3 | 0x83  | 4800 bps |
| DEVIATN | 0x04  | deviation ≈ 2381 Hz |
| PKTCTRL0 | 0x02 | infinite packet mode, no whitening, no CRC |
| SYNC1/0  | 0x08/0x6D | sync word (bit-reversed preamble) |
| IOCFG0   | 0x0D  | GDO0 asserts on sync, deasserts after packet |

### RS41 frame decoding pipeline
1. Bit-reverse each FIFO byte (CC1101 MSB-first ↔ RS41 LSB-first)
2. XOR descramble with 64-byte mask (`RS41_MASK[]`), offset 8 for sync word length
3. Skip bytes 0–47 (Reed-Solomon parity, not corrected) and byte 48 (frame type)
4. Parse TLV blocks from byte 49; each validated with CRC-16/CCITT-FALSE

### XOR mask
The mask in `rs41_decoder.c` is correct for CC1101 bit-reversed data with offset +8.
The "standard" solvespace mask (for SDR/LSB-first data) does **not** work here.

### TLV block structure
```
[1B block_id] [1B block_len] [block_len bytes data] [2B CRC16-CCITT-FALSE LE]
```

Key block IDs:
- `0x79` STATUS: `data[2..9]` = 8-char serial; `data[23]` = calframe index (0–50);
  `data[24..39]` = 16 bytes of calibration payload
- `0x7A` PTU: 12× uint24-LE ADC; `meas[0,1,2]` = T_main/ref1/ref2;
  `meas[3,4,5]` = P_main/ref1/ref2
- `0x7B` GPS: `data[0..11]` = ECEF x/y/z (int32 × 0.01 m);
  `data[12..17]` = ECEF velocities (int16 × 0.01 m/s); `data[19]` = numSats

### Temperature calibration (calframes 3–6)
Each STATUS frame rotates through calframe indices 0–50, carrying 16 bytes.
Calframes 3, 4, 5, 6 contain `Rf1`, `Rf2`, `co1[3]`, `calT1[3]` (float32 LE,
some spanning frame boundaries).  Formula from rs1729/rs41ptu.c:
```
g  = (f2-f1)/(Rf2-Rf1);  Rb = (f1*Rf2-f2*Rf1)/(f2-f1)
Rc = meas[0]/g - Rb;     R  = Rc*calT1[0]
T  = (co1[0] + co1[1]*R + co1[2]*R² + calT1[1]) * (1+calT1[2])
```

### Pressure calibration (calframes 6–7, RS41-SGP only)
Calframes 6 and 7 contain `Fp1`, `Fp2`, `Cp[3]`.  Same formula structure:
```
g  = (fp2-fp1)/(Fp2-Fp1);  Rb = (fp1*Fp2-fp2*Fp1)/(fp2-fp1)
Rc = meas[3]/g - Rb
P  = Cp[0] + Cp[1]*Rc + Cp[2]*Rc²     (hPa)
```

### Rs41Frame struct (rs41_decoder.h)
```c
typedef struct {
    char    id[9];    // sonde serial, NUL-terminated
    float   lat;      // degrees, N > 0
    float   lon;      // degrees, E > 0
    float   alt;      // metres MSL
    uint8_t sats;     // GPS satellites
    float   temp;     // °C; -999.0f = not yet decoded
    float   pres;     // hPa; -1.0f  = not yet decoded
    int16_t rssi;     // dBm
    bool    has_gps;  // lat/lon/alt valid
} Rs41Frame;
```

## Compiler constraints

The Flipper SDK enables `-Werror=double-promotion` and `-Werror=format-truncation`.
- Use `f`-suffix literals: `1013.25f`, `2.25577e-5f`, etc.
- Cast float args to `(double)` for `%f`/`%.1f` in `snprintf`.
- Size `snprintf` buffers conservatively so GCC can prove no truncation.

## Display layout (128×64 px, FontSecondary = 8 px)

```
RS41  405.1 MHz [RX]      ← FontPrimary, y=8
─────────────────────     ← separator line y=10
ID:N1320638  -84dBm       ← y=20
14.6685N  037.0377W       ← y=29
Alt: 1460m T:+19.0C       ← y=38
P: 849.1hPa  Sats: 0      ← y=47
OK:stop UP/DN:freq BACK   ← hint y=62 (clipped at ~21 chars)
```

Fields display `---` until calibration is available.

## Known limitations / future work

- No Reed-Solomon error correction (bytes 0–47 of payload) → ~33% decode rate indoors
- GPS ECEF→LLA uses float Bowring iteration (~1 m accuracy, fine for sondes)
- Pressure only available on RS41-SGP variant (not standard RS41-SG)
