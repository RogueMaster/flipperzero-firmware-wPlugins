#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ufbt_command=${UFBT:-ufbt}
# Retain staging for inspection; never replace the main application's manifest.
stage=$(mktemp -d "${TMPDIR:-/tmp}/fib-sdk-example.XXXXXX")
mkdir -p "$stage/sdk/flipper" "$stage/examples/flipper_bridge_client"
for file in bridge_session.c bridge_session.h bridge_protocol.c bridge_protocol.h usb_transport.c usb_transport.h config.h; do
    cp "$project_root/$file" "$stage/$file"
done
cp "$project_root/sdk/flipper/fib_bridge_client.c" "$project_root/sdk/flipper/fib_bridge_client.h" "$stage/sdk/flipper/"
cp "$project_root/examples/flipper_bridge_client/example_app.c" "$stage/examples/flipper_bridge_client/"
cp "$project_root/examples/flipper_bridge_client/application.fam" "$stage/application.fam"
(cd "$stage" && "$ufbt_command")
printf 'SDK example FAP: %s/dist/fib_sdk_example.fap\n' "$stage"
