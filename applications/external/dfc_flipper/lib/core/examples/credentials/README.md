# DFC v6 examples

These credentials are **synthetic** and use public test keys. They were not
captured from a physical card. Never install their keys on a production card.
Each `.dfc` file and same-named `.dfcb` file represent the same credential.

| Pair | Shows |
| --- | --- |
| [v6-minimal.dfc](v6-minimal.dfc) / [v6-minimal.dfcb](v6-minimal.dfcb) | A small EV3 PICC with a 2TDEA key, ISO and ISO 7816 support, and ISO 7816 preferred. |
| [v6-auth-commands.dfc](v6-auth-commands.dfc) / [v6-auth-commands.dfcb](v6-auth-commands.dfcb) | Multiple applications, 2TDEA and AES command masks, preferred commands, version overrides, data/value/record files, and application secure-messaging disable. |
| [v6-maximal.dfc](v6-maximal.dfc) / [v6-maximal.dfcb](v6-maximal.dfcb) | Every optional field group, every file type, key sets and ordinary keys, all six authentication command choices, and all supported key families. |

The `v6-auth-commands` pair is also an independent vector in the
`dfc-conformance` corpus.
The maximal example's static signature is placeholder bytes and does not prove
chip originality. All three pairs can be checked with `tools/dfc_check.py --pair` in
`dfc-conformance`. DFC core's `make -C tests test` checks exact text/binary
round trips, rejected field reorderings, and simulator commands against each
pair. See the
[full schema](../../docs/credential-schema-v6.md) for every field and tag.
