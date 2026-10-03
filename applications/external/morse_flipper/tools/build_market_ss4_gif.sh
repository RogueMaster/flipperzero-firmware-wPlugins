#!/usr/bin/env bash

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
input="${1:-$root_dir/docs/images/2026-07-31_19-46-00.mp4}"
gif_output="${2:-$root_dir/docs/images/ss4.gif}"
webp_output="${3:-$root_dir/docs/images/ss4.webp}"
palette_source="$root_dir/docs/images/ss4.png"

for command in ffmpeg convert identify; do
    if ! command -v "$command" >/dev/null 2>&1; then
        printf 'missing required command: %s\n' "$command" >&2
        exit 1
    fi
done

if [[ ! -f "$input" ]]; then
    printf 'missing input video: %s\n' "$input" >&2
    exit 1
fi
if [[ ! -f "$palette_source" ]]; then
    printf 'missing palette source: %s\n' "$palette_source" >&2
    exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

# The emulator framebuffer occupies an exact 512x256 region. Start on the first
# score digits, omit the preceding history/wait frames, and hold the result long
# enough to read before the animation loops. GIF timing is measured in
# centiseconds, so 25 fps gives every output frame an exact 4 cs duration.
ffmpeg -hide_banner -loglevel error \
    -i "$input" \
    -map 0:v:0 \
    -vf "trim=start=2.30:end=4.58,setpts=PTS-STARTPTS,crop=512:256:64:82,fps=25,tpad=stop_mode=clone:stop_duration=1" \
    "$tmp_dir/frame-%04d.png"

# Reconstruct the logical 128x64 LCD before remapping colours. H.264 softens the
# edges of the emulator's 4x enlargement; thresholding that enlargement directly
# leaves three-pixel strokes beside intact four-pixel strokes. Collapsing each
# 4x4 block first and enlarging with nearest-neighbour restores the pixel grid.
# Both animation formats are built from these same reconstructed frames.
for frame in "$tmp_dir"/frame-*.png; do
    name="${frame##*/}"
    convert \
        "$frame" \
        -filter Box \
        -resize 128x64\! \
        -colorspace Gray \
        -threshold 30% \
        +level-colors '#000000,#FE8A2C' \
        -filter Point \
        -resize 512x256\! \
        -dither None \
        -remap "$palette_source" \
        "$tmp_dir/mapped-$name"
done

# GIF stores only the changed rectangles between frames.
convert \
    "$tmp_dir"/mapped-frame-*.png \
    -set delay 4 \
    -loop 0 \
    -layers Optimize \
    "$gif_output"

# The WebP is lossless; no additional colour conversion or resampling occurs.
ffmpeg -hide_banner -loglevel error -y \
    -framerate 25 \
    -i "$tmp_dir/mapped-frame-%04d.png" \
    -an \
    -c:v libwebp_anim \
    -lossless 1 \
    -compression_level 6 \
    -loop 0 \
    "$webp_output"

for output in "$gif_output" "$webp_output"; do
    geometry="$(identify -format '%wx%h' "$output[0]")"
    colors="$(convert "$output" -coalesce -append -format '%k' info:)"
    if [[ "$geometry" != "512x256" || "$colors" != "2" ]]; then
        printf 'unexpected animation result: %s geometry=%s colours=%s\n' \
            "$output" "$geometry" "$colors" >&2
        exit 1
    fi
done

printf 'created %s and %s (512x256, 25 fps, two colours)\n' "$gif_output" "$webp_output"
