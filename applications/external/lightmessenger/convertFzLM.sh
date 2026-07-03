#!/bin/bash

# Converts a GIF to up to 10 true 1bpp BMPv1 frames (≤42x16) without dithering.
# Optional -i flag inverts black/white logic.
# Dependencies: ffmpeg, ImageMagick (`convert`)

set -euo pipefail

# --- Dependency checks ---
command -v ffmpeg >/dev/null || { echo "[x] ffmpeg not found. Install it with: sudo apt install ffmpeg"; exit 1; }
command -v convert >/dev/null || { echo "[x] ImageMagick 'convert' not found. Install it with: sudo apt install imagemagick"; exit 1; }

# --- Parse options ---
INVERT=0
while getopts "i" opt; do
  case "$opt" in
    i) INVERT=1 ;;  # -i: Invert black/white logic
  esac
done
shift $((OPTIND - 1))

# --- Check arguments ---
if [ $# -ne 1 ]; then
  echo "Usage: $0 [-i] input.gif"
  exit 1
fi

INPUT_GIF="$1"
[ ! -f "$INPUT_GIF" ] && { echo "[x] File not found: $INPUT_GIF"; exit 1; }

# --- Setup ---
BASE=$(basename "$INPUT_GIF" .gif)
OUTDIR="$BASE"
mkdir -p "$OUTDIR"

# --- Count frames ---
TOTAL_FRAMES=$(ffprobe -v error -select_streams v:0 -count_frames \
  -show_entries stream=nb_read_frames \
  -of default=nokey=1:noprint_wrappers=1 "$INPUT_GIF")
TOTAL_FRAMES=${TOTAL_FRAMES:-1}
FRAME_COUNT=$(( TOTAL_FRAMES < 10 ? TOTAL_FRAMES : 10 ))

# --- Compute evenly spaced frame indices ---
INDICES=()
for i in $(seq 0 $((FRAME_COUNT - 1))); do
  INDICES+=($((i * TOTAL_FRAMES / FRAME_COUNT)))
done

# --- Extract all frames as PNGs ---
ffmpeg -v error -i "$INPUT_GIF" -vsync 0 -start_number 0 "$OUTDIR/${BASE}_raw_%d.png"

# --- Process selected frames ---
COUNT=0
for IDX in "${INDICES[@]}"; do
  RAW="$OUTDIR/${BASE}_raw_${IDX}.png"
  CROP="$OUTDIR/${BASE}_crop_${COUNT}.png"
  OUT="$OUTDIR/${BASE}_${COUNT}.bmp"

  echo "[*] Frame $COUNT ← index $IDX"

  # Resize to 16px height, crop 42x16 from top-left
  convert "$RAW" -resize x16 \
    -gravity NorthWest -crop 42x16+0+0 +repage \
    "$CROP"

  # Threshold and optional inversion
  if [ "$INVERT" -eq 1 ]; then
    FILTER="format=gray,geq=lum_expr='gt(lum(X,Y)\,128)*255'"
  else
    FILTER="format=gray,geq=lum_expr='lte(lum(X,Y)\,128)*255'"
  fi

  # Convert to true 1bpp BMP
  ffmpeg -v error -y -i "$CROP" \
    -vf "$FILTER" \
    -pix_fmt monow "$OUT"

  rm -f "$CROP"
  COUNT=$((COUNT + 1))
done

# --- Cleanup ---
rm -f "$OUTDIR/${BASE}_raw_"*.png

echo "[✓] Done. $COUNT frame(s) written to '$OUTDIR' (true 1bpp BMPv1, no dithering)"
