<!-- Source: https://docs.flipper.net/zero/rfid/animal-microchips -->
<!-- Mirrored: 2026-07-14 -->

# Animal Microchips

## Overview

An animal microchip is "a radio frequency identification (rfid) transponder that stores a unique identification number" roughly the size of a grain of rice. When scanned, it transmits its ID number, which can help locate information about the animal and owner if registered in a database.

The Flipper Zero can read animal microchips using its low-frequency RFID antenna. While designed for 125 kHz operation, it supports reading 134.2 kHz microchips as an additional feature.

## Compatible Microchips

Supported formats include:
- **FDX-B** (15 digit, ISO compliant, including thermo microchips)
- **FDX-A** (10 digit, non-ISO compliant)
- Other chips in animal microchip form factor

**Reading specifications:**
- Primary frequency: 125 kHz
- Extended range: 110–140 kHz (with shorter distance)
- Reading distance: 10 mm (0.39 inch)

## Locating Microchips

### Method 1: Manual Palpation
Feel between the shoulder blades and left neck midway region for a solid, rice-grain-sized object just under the skin. Note that microchips may migrate from original implantation sites.

### Method 2: Flipper Zero Scanning
1. Go to Main Menu > 125 kHz RFID > Extra Actions
2. Select "Read ASK" and press OK
3. Position the device's back against the pet's skin
4. Move systematically in a grid pattern, holding 3 seconds at each location
5. If unsuccessful, rotate 90° and repeat

## Troubleshooting

If detection fails:
- Try smaller grid steps with longer hold times
- Scan in different locations (microchips migrate)
- Move away from metal objects or electronic equipment
- Check if the animal has a high-frequency microchip (use NFC app)
- Verify microchip type compatibility

## Data Structure

ISO-compliant microchips display a 15-digit ID where the first three digits represent either an ISO 3166-1 country code or manufacturer code, with remaining digits as the unique microchip number.

## Finding Animal Information

Search pet recovery databases using the microchip ID:
- American Animal Hospital Association (US/Canada)
- EuroPetNet (Europe)
- PetMaxx (Worldwide)
