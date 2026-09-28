<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# FASC-N identifiers

Include `<tiny_crypto/fascn.h>` and enable `TINY_CRYPTO_ENABLE_FASCN`.
`TC_FASCN_read` decodes a 25-byte FASC-N into fixed-width numeric fields.
It checks the start/end sentinels, field separators, decimal digits, odd parity
and longitudinal checksum defined in
[PACS TIG v2.3, sections 6.1–6.3](https://www.idmanagement.gov/docs/pacs-tig-scepacs.pdf).

Agency, system and organization have four decimal digits; credential has six,
and person has ten. Series, issue, category and association each have one.
The structure stores their numeric values. `TC_FASCN_write` restores leading
zeros and writes exactly `TC_FASCN_BYTES`, including parity and checksum.
Values exceeding a field's width return `TC_TLV_INVALID`. A short output buffer
returns `TC_TLV_LIMIT`.

Both functions change output only on `TC_TLV_OK`. Input and output storage must
be disjoint. Decoding produces an independent value with no borrowed pointers.
The codec uses fixed-size local storage.

```c
TC_FASCN fields;
uint8_t encoded[TC_FASCN_BYTES];
TC_TLV_result result = TC_FASCN_read(input, &fields);
if (result != TC_TLV_OK) return result;
result = TC_FASCN_write(&fields, encoded, sizeof encoded);
if (result != TC_TLV_OK) return result;
```

Field syntax is separate from issuer and credential policy. Numeric category
values still require application checks. Authenticate the containing object and
apply the selected PIV/TWIC identifier policy before using the identifier.

## NEXGEN TWIC UUIDs

Include `<tiny_crypto/twic_uuid.h>`. `TC_TWIC_uuid_read` checks the NEXGEN
namespace, version, variant and reserved bits, then returns the 14-digit decimal
agency/system/credential number as `uint64_t`. `TC_TWIC_uuid_write` produces
the corresponding 16-byte UUID. The number must be at most 99999999999999.
Both functions preserve output on error.

`TC_TWIC_uuid_match` compares a UUID with a decoded FASC-N's first three fields.
Check for `TC_TLV_OK` before using its match result. Series, issue and person
fields are absent from the UUID and remain part of the full FASC-N identity.
Select this binding for the TWIC application. PIV-I activation can replace the
PIV application's FASC-N with a placeholder and requires a separate application
policy. The mapping is defined in TWIC Part 2 v5 Appendix D.
