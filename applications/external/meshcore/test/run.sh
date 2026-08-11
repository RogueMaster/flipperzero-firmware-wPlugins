#!/usr/bin/env bash
# Host protocol tests — POSIX twin of run.ps1, for CI and for anyone not on
# Windows. Same compiler, same flags, same expectations.
#
#     ./test/run.sh
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/build/test"
mkdir -p "$out"

cc=(python -m ziglang cc)
common=(-std=c11 -g "-I$root/test/shims")
# Our own code is held to the same bar as the firmware build.
strict=(-Wall -Wextra -Werror)

compile() { # src obj [extra flags...]
    local src="$1" obj="$2"; shift 2
    "${cc[@]}" "${common[@]}" "$@" -c "$src" -o "$obj"
}

# The vendored library is upstream code: compiled, but not held to our warning
# settings, so a clang-only warning there cannot block our tests.
compile "$root/protocol/meshcore_c/meshcore_companion.c" "$out/meshcore_companion.o"
compile "$root/protocol/meshcore_link.c"     "$out/meshcore_link.o"     "${strict[@]}"
compile "$root/protocol/meshcore_route.c"    "$out/meshcore_route.o"    "${strict[@]}"
compile "$root/messenger/meshcore_contacts.c" "$out/meshcore_contacts.o" "${strict[@]}"
compile "$root/messenger/meshcore_messages.c" "$out/meshcore_messages.o" "${strict[@]}"
compile "$root/logger/meshcore_rxlog.c"      "$out/meshcore_rxlog.o"    "${strict[@]}"
compile "$root/logger/meshcore_telemetry.c"  "$out/meshcore_telemetry.o" "${strict[@]}"
compile "$root/logger/meshcore_ping.c"       "$out/meshcore_ping.o"     "${strict[@]}"
compile "$root/logger/meshcore_events.c"     "$out/meshcore_events.o"   "${strict[@]}"
compile "$root/config/meshcore_json.c"       "$out/meshcore_json.o"     "${strict[@]}"
compile "$root/config/meshcore_preset.c"     "$out/meshcore_preset.o"   "${strict[@]}"
compile "$root/config/meshcore_apply.c"      "$out/meshcore_apply.o"    "${strict[@]}"
compile "$root/test/fakes.c"                 "$out/fakes.o"             "${strict[@]}"
compile "$root/test/test_meshcore.c"         "$out/test_meshcore.o"     "${strict[@]}"

"${cc[@]}" \
    "$out/meshcore_companion.o" "$out/meshcore_link.o" "$out/meshcore_route.o" \
    "$out/meshcore_contacts.o" "$out/meshcore_messages.o" "$out/meshcore_rxlog.o" \
    "$out/meshcore_telemetry.o" "$out/meshcore_ping.o" "$out/meshcore_events.o" \
    "$out/meshcore_json.o" "$out/meshcore_preset.o" "$out/meshcore_apply.o" \
    "$out/fakes.o" "$out/test_meshcore.o" \
    -o "$out/test_meshcore"

"$out/test_meshcore"
