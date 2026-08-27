#!/usr/bin/env bash
# Print a stable hash of the sources that produce the ESP firmware images.
#
# The .bin images bundled in the fap (flipper/hotspot-arcade/assets/firmware/) are
# committed build outputs, so nothing stops them from going stale when the ESP source
# changes — and a stale image means "Install Firmware" flashes firmware the app then
# rejects as outdated. We can't diff the images themselves: ESP-IDF embeds a build
# timestamp in the app descriptor, so two builds of identical source differ byte-wise.
# Instead we hash the *inputs* and record that alongside the images; CI compares.
#
# tools/build-fap.sh writes the result to flipper/hotspot-arcade/.bundled-fw.sha256
# right after it refreshes the images. The build.yml "bundled-assets" job recomputes it
# and fails if the two disagree.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

# Hash every file that feeds the firmware build: the sketch plus the vendored libs.
#
# Deliberately `git ls-files` and not `find`: the stamp has to come out identical on a
# dev machine and on a CI runner, and a working-tree walk doesn't. It would sweep up
# untracked and ignored strays (a .DS_Store, the build/ output dir, a scratch file) that
# exist in one place and not the other, so the stamp would mismatch for reasons that have
# nothing to do with the firmware. Tracked files are the same set everywhere.
# The arduino core is an input too, and an invisible one: the same sources built on a
# different core produce different images while every tracked file is untouched. Without
# this the guard would sail straight past a core bump with a stale image set committed.
CORE_VER="$(sed -n 's/^CORE_VER="\(.*\)"$/\1/p' tools/build-fap.sh | head -1)"

{
    printf 'core %s\n' "$CORE_VER"
    git ls-files -z -- esp32/hotspot-arcade-fw esp32/libs |
        LC_ALL=C sort -z | xargs -0 shasum -a 256
} | shasum -a 256 | cut -d' ' -f1
