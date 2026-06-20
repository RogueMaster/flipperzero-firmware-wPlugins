# enhauto-re

Research notes used while building out this project's roadmap. The goal of
the work in this folder was to map the **complete set of CAN-bus
capabilities** an aftermarket Tesla CAN dongle exposes — so that we know
what's possible to add to a free / open-source MCP2515-based tool, and
which "vendor-secret" features turn out to be re-implementations of public
Tesla CAN signals or simple Tesla Fleet API calls.

## Documents

| File | What it covers |
|------|----------------|
| [`COMMANDS.md`](COMMANDS.md) | The master command catalogue. 98 physical actions + 73 automation toggles, each classified by delivery channel (Tesla Fleet HTTP API / CAN injection / read-only sniff) and cross-referenced to public Tesla CAN sources where the frame template is known. |
| [`COMMANDER_WIFI_PANDA.md`](COMMANDER_WIFI_PANDA.md) | Documents the (publicly-known) finding that the enhauto S3XY Commander exposes a comma.ai Panda UDP protocol over its WiFi AP — meaning every CAN frame the dongle sees on the car is observable from any laptop / phone / Pi without needing the official phone app. Wire protocol details. |
| [`CAN_DICTIONARY.md`](CAN_DICTIONARY.md) | Cross-source Tesla CAN signal map built from mikegapinski/tesla-can-explorer (40k signals from `libQtCarVAPI.so`), talas9/tesla_can_signals (per-model wire format), tuncasoftbildik/tesla-can-mod (working templates), and commaai/opendbc. This is the lookup table the rest of this project uses to identify CAN frames. |

## Scope notes

- These documents describe **what is possible** — they are not a
  re-implementation of any third-party product, and do not redistribute
  any binary, protobuf descriptor, or other copyrighted material from a
  third party.
- Any specific CAN frame templates referenced come from
  publicly-published Tesla CAN research projects, cited inline.
- The Tesla Fleet HTTP API endpoint references come directly from
  Tesla's own [Fleet API documentation](https://developer.tesla.com/docs/fleet-api).
