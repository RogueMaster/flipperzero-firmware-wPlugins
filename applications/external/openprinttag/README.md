# OpenPrintTag Reader for Flipper Zero

[![Build FAP](https://github.com/Houzvicka/FlipperPrintTag/actions/workflows/build.yml/badge.svg)](https://github.com/Houzvicka/FlipperPrintTag/actions/workflows/build.yml)

A Flipper Zero application to read and write OpenPrintTag NFC tags used for 3D printing filament spools.

## About OpenPrintTag

OpenPrintTag is an open-source NFC tag standard for 3D printing materials, developed by Prusa Research. It stores material information like brand, type, color, and filament usage data on NFC tags attached to filament spools.

- **Specification**: https://specs.openprinttag.org/
- **GitHub Repository**: https://github.com/prusa3d/OpenPrintTag

## Features

- ✅ **NFC Tag Detection**: Automatic scanning for ISO15693 (NFC-V) tags
- ✅ **Complete Tag Reading**: Reads all memory blocks from ISO15693 tags
- ✅ **NDEF Parsing**: Extracts NDEF records from tag memory
- ✅ **CBOR Decoding**: Full CBOR parser for OpenPrintTag format
- ✅ **Material Identification**: Displays material class (FFF/SLA) and type (PLA, PETG, TPU, etc.)
- ✅ **Temperature Information**: Shows print and bed temperature ranges
- ✅ **Weight & Length**: Displays filament weight and length
- ✅ **Usage Tracking**: Shows consumed weight and calculates remaining material
- ✅ **Workgroup Support**: Displays workgroup information for shared materials
- 🚧 Write/update auxiliary section (planned)
- 🚧 Create new OpenPrintTag tags (planned)

## Data Format

OpenPrintTag uses a hierarchical format:
- **NDEF Record**: MIME type `application/vnd.openprinttag`
- **CBOR Encoding**: Three sections encoded as CBOR maps
  - **Meta Section**: Region offsets and sizes
  - **Main Section**: Static material info (brand, material name, type, GTIN)
  - **Auxiliary Section**: Dynamic data (remaining length, used length, timestamp)

## Installation

### Prerequisites

- Flipper Zero with updated firmware
- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) or the full Flipper Zero firmware build environment

### Building with ufbt

```bash
# Clone this repository
git clone https://github.com/Houzvicka/FlipperPrintTag
cd FlipperPrintTag

# Build the application
python -m ufbt

# Install to connected Flipper Zero
python -m ufbt launch
```

**Build Output:**
- `dist/openprinttag.fap` - Compiled application file
- Target: Flipper Zero f7
- API: 86.0

### Building with full firmware

```bash
# Clone into the applications_user folder of your firmware
cd applications_user
git clone <your-repo-url> openprinttag

# Build firmware with this app included
cd ..
./fbt fap_openprinttag

# Copy to Flipper
./fbt launch_app APPSRC=openprinttag
```

## Usage

1. Open the **OpenPrintTag** app from the NFC category on your Flipper Zero
2. Select **Read OpenPrintTag** from the menu
3. Place your Flipper Zero near an OpenPrintTag NFC tag (typically on filament spool)
4. Wait for tag detection (ISO15693/NFC-V tags)
5. View the parsed material information

### Example Output

```
OpenPrintTag Data

Brand: Prusa
Material: PLA Galaxy Black
Class: FFF
Type: PLA
Weight: 1000 g
Diameter: 1.75 mm
Length: 330.0 m
Nozzle: 200-230°C
Bed: 50-60°C

--- Usage Data ---
Consumed: 250 g
Remaining: 750 g
```

## Project Structure

```
FlipperPrintTag/
├── application.fam              # App manifest
├── openprinttag.c               # Main app entry point
├── openprinttag_i.h             # Internal header with data structures
├── openprinttag_fields.h        # OpenPrintTag field key definitions
├── cbor_parser.h/c              # CBOR decoder implementation
├── ndef_parser.c                # NDEF record parser
├── openprinttag_parser.c        # OpenPrintTag-specific CBOR parser
├── material_types.h             # Material class and type enums
├── openprinttag.png             # App icon (10x10px)
├── scenes/                      # Scene implementations
│   ├── openprinttag_scene_start.c
│   ├── openprinttag_scene_read.c         # NFC scanner and poller
│   ├── openprinttag_scene_read_success.c
│   ├── openprinttag_scene_read_error.c
│   └── openprinttag_scene_display.c      # Data display
└── dist/                        # Build output
    └── openprinttag.fap         # Compiled application
```

## Implementation Details

### What's Implemented

✅ **NFC Stack Integration**
- Uses Flipper's NFC Scanner API for automatic tag detection
- NFC Poller reads all memory blocks from ISO15693 tags
- Supports standard NFC-V tags (ICODE SLIX, etc.)

✅ **Data Parsing**
- Complete CBOR decoder with support for all OpenPrintTag data types
- NDEF TLV structure parser
- Correct field key mappings per official specification

✅ **Material Database**
- 39 material types (PLA, PETG, TPU, ABS, ASA, PC, etc.)
- 2 material classes (FFF Filament, SLA Resin)
- Human-readable names and abbreviations

✅ **Display Information**
- Brand and material identification
- Print and bed temperature ranges
- Filament diameter and length
- Weight information with usage tracking
- Workgroup support for shared materials

### Limitations

1. **Write Support**: Writing/updating tags is not yet implemented
2. **Advanced Fields**: Some optional fields (colors, tags array, UUIDs) are parsed but not displayed
3. **SLA-Specific**: SLA resin fields (viscosity, cure wavelength) parsed but not shown in UI

### Future Enhancements

- Write support for updating auxiliary section (consumed weight, timestamps)
- Display material tags (properties like "abrasive", "conductive", etc.)
- Show color information (primary/secondary colors)
- Support for creating new OpenPrintTag tags
- Vendor-specific field handling

## OpenPrintTag Field Reference

### Meta Section (Offsets and Sizes)
- `0` - Main region offset
- `1` - Main region size
- `2` - Auxiliary region offset
- `3` - Auxiliary region size

### Main Section (Static Material Data)
- `4` - GTIN (Global Trade Item Number)
- `8` - Material class (0=FFF, 1=SLA)
- `9` - Material type (0=PLA, 1=PETG, 2=TPU, etc.)
- `10` - Material name (string)
- `11` - Brand name (string)
- `14` - Manufactured date (UNIX timestamp)
- `15` - Expiration date (UNIX timestamp)
- `16` - Nominal netto full weight (grams)
- `17` - Actual netto full weight (grams)
- `18` - Empty container weight (grams)
- `30` - Filament diameter (mm)
- `34` - Min print temperature (°C)
- `35` - Max print temperature (°C)
- `37` - Min bed temperature (°C)
- `38` - Max bed temperature (°C)
- `53` - Nominal full length (mm)
- `54` - Actual full length (mm)

### Auxiliary Section (Dynamic Usage Data)
- `0` - Consumed weight (grams)
- `1` - Workgroup identifier (string, max 8 chars)
- `2` - General purpose range user (string)
- `3` - Last stir time (UNIX timestamp, for SLA resins)

## Contributing

Contributions are welcome! Priority areas:
- **Write Support**: Implement auxiliary section updates (consumed weight, timestamps)
- **Material Tags**: Display material property tags (68 defined tags for properties like abrasive, conductive, etc.)
- **Color Display**: Show primary and secondary color information
- **Extended Fields**: Display UUIDs, manufacturer dates, and vendor-specific fields
- **Error Handling**: Improved validation and error messages
- **Testing**: Test with various OpenPrintTag implementations

See [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) for detailed implementation notes.

## License

This project is provided as-is for educational and development purposes. The OpenPrintTag specification is open-source and free to implement.

## Resources

- [Flipper Zero Documentation](https://docs.flipper.net/)
- [OpenPrintTag Specification](https://specs.openprinttag.org/)
- [CBOR Specification](https://cbor.io/)
- [NFC Forum NDEF Specification](https://nfc-forum.org/)
