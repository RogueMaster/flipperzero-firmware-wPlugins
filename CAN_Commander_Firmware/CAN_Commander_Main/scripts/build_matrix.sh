#!/bin/sh
# Compiles the supported firmware configurations with arduino-cli.
#
# Usage: scripts/build_matrix.sh [output_dir]
#
# Requires: arduino-cli with the esp32 core (tested 3.3.4) and the libraries listed in
# README.md (ACAN2517FD, autowp-mcp2515, Adafruit NeoPixel).
set -u

SKETCH="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-${TMPDIR:-/tmp}/can_commander_build_matrix}"
mkdir -p "$OUT"

S3="esp32:esp32:esp32s3:CDCOnBoot=cdc"
CLASSIC="esp32:esp32:esp32"
failures=0

# build <name> <expect: pass|fail> <fqbn> [extra flags] [expected failure text]
build() {
  name="$1"
  expect="$2"
  fqbn="$3"
  flags="${4:-}"
  fail_text="${5:-}"
  log="$OUT/$name.log"

  if [ -n "$flags" ]; then
    arduino-cli compile --warnings all --build-path "$OUT/bp_$name" --fqbn "$fqbn" \
      --build-property "compiler.cpp.extra_flags=$flags" "$SKETCH" >"$log" 2>&1
  else
    arduino-cli compile --warnings all --build-path "$OUT/bp_$name" --fqbn "$fqbn" "$SKETCH" >"$log" 2>&1
  fi
  rc=$?

  if [ "$expect" = "pass" ] && [ $rc -eq 0 ]; then
    ram=$(grep -o 'Global variables use [0-9]* bytes' "$log" | grep -o '[0-9]*')
    echo "PASS  $name (static RAM ${ram} B)"
  elif [ "$expect" = "fail" ] && [ $rc -ne 0 ] && grep -q "$fail_text" "$log"; then
    echo "PASS  $name (failed as expected: $fail_text)"
  else
    echo "FAIL  $name (exit $rc, expected $expect) -- see $log"
    failures=$((failures + 1))
  fi
}

build s3_default pass "$S3"
build s3_mcp2515_custom_spi pass "$S3" \
  "-DBOARDCFG_CAN1_DRIVER=2 -DBOARDCFG_CAN1_CRYSTAL_MHZ=16 -DBOARDCFG_SPI_CS_PIN=14 -DBOARDCFG_SPI_SCK_PIN=15 -DBOARDCFG_SPI_MISO_PIN=16 -DBOARDCFG_SPI_MOSI_PIN=17"
build s3_mcp2518fd_40mhz pass "$S3" "-DBOARDCFG_CAN1_CRYSTAL_MHZ=40"
build s3_minimal pass "$S3" "-DBOARDCFG_CAN1_DRIVER=0 -DBOARDCFG_NEOPIXEL_ENABLED=0 -DBOARDCFG_WIFI_AP_ENABLED=0"
# docs/BUILD_YOUR_OWN.md: ESP32-S3 dev board + SN65HVD230, single bus
build diy_s3_single_bus pass "$S3" \
  "-DBOARDCFG_CAN1_DRIVER=0 -DBOARDCFG_NEOPIXEL_ENABLED=0 -DBOARDCFG_FLIPPER_UART_TX_PIN=17 -DBOARDCFG_FLIPPER_UART_RX_PIN=18"
build diy_s3_single_bus_led pass "$S3" \
  "-DBOARDCFG_CAN1_DRIVER=0 -DBOARDCFG_NEOPIXEL_ENABLED=1 -DBOARDCFG_NEOPIXEL_PIN=48 -DBOARDCFG_NEOPIXEL_COUNT=1 -DBOARDCFG_FLIPPER_UART_TX_PIN=17 -DBOARDCFG_FLIPPER_UART_RX_PIN=18"
build esp32_classic pass "$CLASSIC"
build esp32_classic_mcp2515 pass "$CLASSIC" "-DBOARDCFG_CAN1_DRIVER=2"
# The old README command replaced build.extra_flags, dropping USB CDC; it must not
# silently produce a firmware whose CLI shares the Flipper UART pins.
arduino_readme_flags() {
  log="$OUT/s3_readme_command.log"
  arduino-cli compile --build-path "$OUT/bp_s3_readme_command" --fqbn esp32:esp32:esp32s3 \
    --build-property "build.extra_flags=-DESP32" "$SKETCH" >"$log" 2>&1
  rc=$?
  if [ $rc -ne 0 ] && grep -q "USB CDC On Boot" "$log"; then
    echo "PASS  s3_readme_command (rejected at compile time, as expected)"
  else
    echo "FAIL  s3_readme_command (exit $rc; expected a USB CDC compile error) -- see $log"
    failures=$((failures + 1))
  fi
}
arduino_readme_flags

echo "$failures failure(s)"
[ $failures -eq 0 ]
