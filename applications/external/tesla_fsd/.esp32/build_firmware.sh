#!/usr/bin/env bash
# Tesla FSD ESP32 firmware packager
# Builds one PlatformIO environment and creates:
#   1) a merged full-flash image for first USB flashing
#   2) an app-only image for Web UI OTA updates
#   3) SHA256SUMS.txt with checksums for both firmware images

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

ENV_NAME="${PIO_ENV:-waveshare-s3-can}"
CLEAN=0

usage() {
    cat <<'EOF'
Usage:
  ./build_firmware.sh [env]
  ./build_firmware.sh -e <env> [--clean]

Examples:
  ./build_firmware.sh
  ./build_firmware.sh waveshare-s3-can
  ./build_firmware.sh -e m5stack-atom --clean

Outputs:
  build_output/firmware_<env>_<timestamp>/
    <env>_full_<timestamp>.bin   merged full-flash image, USB first flash
    <env>_ota_<timestamp>.bin    app-only firmware, Web UI OTA upload
    SHA256SUMS.txt               SHA-256 checksums for both .bin files
    bootloader.bin
    partitions.bin
    boot_app0.bin
    <env>_<timestamp>.elf

Available envs in this project:
  waveshare-s3-can
  m5stack-atom
  m5stack-atom-swap-pins
  esp32-mcp2515
  esp32-lilygo
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -e|--env)
            ENV_NAME="$2"
            shift 2
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        waveshare-esp32s3-can)
            ENV_NAME="waveshare-s3-can"
            shift
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage
            exit 2
            ;;
        *)
            ENV_NAME="$1"
            shift
            ;;
    esac
done

case "$ENV_NAME" in
    waveshare-s3-can)
        CHIP="esp32s3"
        BOOT_OFFSET="0x0000"
        FLASH_SIZE="8MB"
        FLASH_FREQ="80m"
        FLASH_MODE="dio"
        BAUD="921600"
        ;;
    m5stack-atom|m5stack-atom-swap-pins|esp32-mcp2515|esp32-lilygo)
        CHIP="esp32"
        BOOT_OFFSET="0x1000"
        FLASH_SIZE="4MB"
        FLASH_FREQ="40m"
        FLASH_MODE="dio"
        BAUD="1500000"
        ;;
    *)
        echo "Unsupported env: $ENV_NAME" >&2
        echo "Run ./build_firmware.sh --help for the supported list." >&2
        exit 2
        ;;
esac

if ! command -v pio >/dev/null 2>&1; then
    echo "PlatformIO CLI 'pio' was not found in PATH." >&2
    exit 1
fi

ESPTOOL=()
if [[ -f "$HOME/.platformio/packages/tool-esptoolpy/esptool.py" ]]; then
    ESPTOOL=(python3 "$HOME/.platformio/packages/tool-esptoolpy/esptool.py")
elif command -v esptool.py >/dev/null 2>&1; then
    ESPTOOL=(esptool.py)
else
    ESPTOOL=(python3 -m esptool)
fi

BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
if [[ ! -f "$BOOT_APP0" ]]; then
    BOOT_APP0="$(find "$HOME/.platformio/packages" -path '*/tools/partitions/boot_app0.bin' -print -quit 2>/dev/null || true)"
fi
if [[ -z "$BOOT_APP0" || ! -f "$BOOT_APP0" ]]; then
    echo "Could not find boot_app0.bin under ~/.platformio/packages." >&2
    echo "Run 'pio run -e $ENV_NAME' once to install the ESP32 framework package." >&2
    exit 1
fi

TIMESTAMP="$(date +"%Y%m%d_%H%M%S")"
BUILD_DIR="$SCRIPT_DIR/.pio/build/$ENV_NAME"
OUT_ROOT="$SCRIPT_DIR/build_output"
OUT_DIR="$OUT_ROOT/firmware_${ENV_NAME}_${TIMESTAMP}"

FULL_BIN="$OUT_DIR/${ENV_NAME}_full_${TIMESTAMP}.bin"
OTA_BIN="$OUT_DIR/${ENV_NAME}_ota_${TIMESTAMP}.bin"
ELF_OUT="$OUT_DIR/${ENV_NAME}_${TIMESTAMP}.elf"

echo "======================================"
echo "Tesla FSD ESP32 firmware build"
echo "Env:       $ENV_NAME"
echo "Chip:      $CHIP"
echo "Timestamp: $TIMESTAMP"
echo "======================================"

if [[ "$CLEAN" == "1" ]]; then
    echo
    echo "[0/4] Cleaning PlatformIO environment"
    pio run -e "$ENV_NAME" -t clean
fi

echo
echo "[1/4] Building PlatformIO environment"
pio run -e "$ENV_NAME"

for required in "$BUILD_DIR/firmware.bin" "$BUILD_DIR/firmware.elf" "$BUILD_DIR/bootloader.bin" "$BUILD_DIR/partitions.bin"; do
    if [[ ! -f "$required" ]]; then
        echo "Missing build artifact: $required" >&2
        exit 1
    fi
done

echo
echo "[2/4] Creating output directory"
mkdir -p "$OUT_DIR"

echo
echo "[3/4] Packaging OTA and debug artifacts"
cp "$BUILD_DIR/firmware.bin" "$OTA_BIN"
cp "$BUILD_DIR/firmware.elf" "$ELF_OUT"
cp "$BUILD_DIR/bootloader.bin" "$OUT_DIR/bootloader.bin"
cp "$BUILD_DIR/partitions.bin" "$OUT_DIR/partitions.bin"
cp "$BOOT_APP0" "$OUT_DIR/boot_app0.bin"

echo
echo "[4/4] Creating merged full-flash image"
"${ESPTOOL[@]}" --chip "$CHIP" merge_bin \
    -o "$FULL_BIN" \
    --flash_mode "$FLASH_MODE" \
    --flash_freq "$FLASH_FREQ" \
    --flash_size "$FLASH_SIZE" \
    "$BOOT_OFFSET" "$OUT_DIR/bootloader.bin" \
    0x8000 "$OUT_DIR/partitions.bin" \
    0xe000 "$OUT_DIR/boot_app0.bin" \
    0x10000 "$OTA_BIN"

cp "$FULL_BIN" "$OUT_ROOT/firmware-full.bin"
cp "$OTA_BIN" "$OUT_ROOT/firmware-ota.bin"

CHECKSUM_CMD=()
if command -v shasum >/dev/null 2>&1; then
    CHECKSUM_CMD=(shasum -a 256)
elif command -v sha256sum >/dev/null 2>&1; then
    CHECKSUM_CMD=(sha256sum)
else
    echo "Could not find 'shasum' or 'sha256sum' to create SHA256SUMS.txt." >&2
    exit 1
fi

(
    cd "$OUT_DIR"
    LC_ALL=C LANG=C "${CHECKSUM_CMD[@]}" "$(basename "$FULL_BIN")" "$(basename "$OTA_BIN")" > SHA256SUMS.txt
)

cp "$OUT_DIR/SHA256SUMS.txt" "$OUT_ROOT/SHA256SUMS.txt"

cat > "$OUT_DIR/README.txt" <<EOF
Tesla FSD ESP32 firmware package

Environment: $ENV_NAME
Chip:        $CHIP
Timestamp:   $TIMESTAMP

Files:
  $(basename "$FULL_BIN")
    Full merged image for first USB flashing. Includes bootloader, partition table,
    OTA boot data, and app firmware.

  $(basename "$OTA_BIN")
    App-only image for Web UI OTA updates. Upload this file at http://192.168.4.1.

  SHA256SUMS.txt
    SHA-256 checksums for the full-flash and OTA firmware images.

USB first flash, single merged file:
  esptool.py --chip $CHIP --port <PORT> --baud $BAUD \\
    --before default_reset --after hard_reset write_flash -z \\
    0x0 $(basename "$FULL_BIN")

USB first flash, separated files:
  esptool.py --chip $CHIP --port <PORT> --baud $BAUD \\
    --before default_reset --after hard_reset write_flash -z \\
    --flash_mode $FLASH_MODE --flash_freq $FLASH_FREQ --flash_size $FLASH_SIZE \\
    $BOOT_OFFSET bootloader.bin \\
    0x8000 partitions.bin \\
    0xe000 boot_app0.bin \\
    0x10000 $(basename "$OTA_BIN")

Web UI OTA:
  1. Connect to WiFi AP: Tesla-FSD
  2. Open: http://192.168.4.1
  3. Upload: $(basename "$OTA_BIN")

Latest shortcut copies are also written to:
  build_output/firmware-full.bin
  build_output/firmware-ota.bin
  build_output/SHA256SUMS.txt
EOF

echo
echo "======================================"
echo "Build complete"
echo "======================================"
echo "Output directory:"
echo "  $OUT_DIR"
echo
ls -lh "$OUT_DIR"
echo
echo "Use for USB first flash:"
echo "  $FULL_BIN"
echo
echo "Use for Web UI OTA:"
echo "  $OTA_BIN"
echo
echo "Latest shortcuts:"
echo "  $OUT_ROOT/firmware-full.bin"
echo "  $OUT_ROOT/firmware-ota.bin"
echo "  $OUT_ROOT/SHA256SUMS.txt"
