<#
    Builds and runs the host protocol tests.

    Uses Zig as the C compiler because it installs with `pip install ziglang`
    and needs no admin rights or MSYS2 — see test/README.md.

        pwsh test/run.ps1
#>
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'build/test'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$cc = @('python', '-m', 'ziglang', 'cc')
$common = @('-std=c11', '-g', "-I$root/test/shims")
# Our own code is held to the same bar as the firmware build.
$strict = @('-Wall', '-Wextra', '-Werror')

function Compile($src, $obj, $extra) {
    $args = $cc[1..($cc.Length - 1)] + $common + $extra + @('-c', $src, '-o', $obj)
    & $cc[0] @args
    if ($LASTEXITCODE -ne 0) { throw "compile failed: $src" }
}

# The vendored library is upstream code: compiled, but not held to our warning
# settings, so a clang-only warning there cannot block our tests.
Compile "$root/protocol/meshcore_c/meshcore_companion.c" "$out/meshcore_companion.o" @()
Compile "$root/protocol/meshcore_link.c" "$out/meshcore_link.o" $strict
Compile "$root/protocol/meshcore_route.c" "$out/meshcore_route.o" $strict
Compile "$root/messenger/meshcore_contacts.c" "$out/meshcore_contacts.o" $strict
Compile "$root/messenger/meshcore_share_uri.c" "$out/meshcore_share_uri.o" $strict
Compile "$root/messenger/meshcore_messages.c" "$out/meshcore_messages.o" $strict
Compile "$root/logger/meshcore_rxlog.c" "$out/meshcore_rxlog.o" $strict
Compile "$root/logger/meshcore_telemetry.c" "$out/meshcore_telemetry.o" $strict
Compile "$root/logger/meshcore_ping.c" "$out/meshcore_ping.o" $strict
Compile "$root/logger/meshcore_events.c" "$out/meshcore_events.o" $strict
Compile "$root/config/meshcore_json.c" "$out/meshcore_json.o" $strict
Compile "$root/config/meshcore_preset.c" "$out/meshcore_preset.o" $strict
Compile "$root/config/meshcore_apply.c" "$out/meshcore_apply.o" $strict
Compile "$root/test/fakes.c" "$out/fakes.o" $strict
Compile "$root/test/test_meshcore.c" "$out/test_meshcore.o" $strict

$exe = Join-Path $out 'test_meshcore.exe'
$linkArgs = $cc[1..($cc.Length - 1)] + @(
    "$out/meshcore_companion.o", "$out/meshcore_link.o", "$out/meshcore_route.o",
    "$out/meshcore_contacts.o", "$out/meshcore_share_uri.o", "$out/meshcore_messages.o", "$out/meshcore_rxlog.o",
    "$out/meshcore_telemetry.o", "$out/meshcore_ping.o", "$out/meshcore_events.o",
    "$out/meshcore_json.o", "$out/meshcore_preset.o", "$out/meshcore_apply.o",
    "$out/fakes.o", "$out/test_meshcore.o", '-o', $exe)
& $cc[0] @linkArgs
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

& $exe
exit $LASTEXITCODE
