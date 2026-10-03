<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# PIV card commands

Enable `TINY_CRYPTO_ENABLE_PIV_COMMAND=ON` and include
`<tiny_crypto/piv_command.h>`. The module sends the PIV and TWIC card commands
of SP 800-73-5 Part 2 and TWIC Part 2 v5 section 5 over the
[APDU channel](apdu.md): SELECT, GET DATA and VERIFY. It checks the answers,
tracks the link state that access rules depend on and gives status words their
PIV or TWIC meaning. It needs the APDU codec and the TLV readers, allocates
nothing and owns no I/O, reader selection or PIN entry.

## Link

A `TC_PIV_link` is one card session over a transport. `TC_PIV_link_init` takes
`TC_PIV_link_options`:

- `channel`: the APDU channel options. The exchange budget covers every
  command of the session, including GET RESPONSE steps. The card limits may
  start at 0, since `TC_PIV_select` applies the limits the card reports.
- `interface`: `TC_PIV_CONTACT` or `TC_PIV_CONTACTLESS`. The application states
  it, because the PIN rules depend on it.
- `response_ne`: Ne for GET DATA. 0 selects 256, the Le `00` of Part 2 section
  3.1.2. An EXTENDED link may request up to 65536.

The command scratch buffer holds one encoded command and is wiped after every
transmit, since VERIFY carries PIN digits. SHORT links need
`TC_APDU_SHORT_COMMAND_MAX_BYTES` (261) bytes. EXTENDED links need
`TC_PIV_EXTENDED_SCRATCH_BYTES`, or the card limit when smaller. It holds one
command of `TC_PIV_COMMAND_MAX_NC` data bytes or the plain secure messaging key
establishment request of `TC_PIV_SM_KEY_REQUEST_BYTES` (80 for CS2, 112 for
CS7), whichever is larger. `TC_PIV_COMMAND_MAX_NC` is the largest command that
a link protects: 32 bytes, or the key proof template size with
`TINY_CRYPTO_ENABLE_PIV_KEY_PROOF`.

`TC_PIV_link_info_get` reports the interface, the selected application and
profile, the secure messaging suite the application announced, and whether the
link is secured, has lost its secure messaging session, has the VCI, or has a
verified PIN. `TC_PIV_link_status` returns the status word of the last command
the card completed. `TC_PIV_link_clear` wipes the scratch and the link. Call it
on every exit path.

Every function returns a `TC_PIV_result`. The first six values follow
`TC_APDU_result`. `TC_PIV_CARD_STATUS` means the card completed the command with
a status other than success, and `TC_PIV_link_status` holds that status.
`TC_PIV_REFUSED` means a safety or state rule stopped the command before it was
sent. A transport failure stops the link, and every later command returns
`TC_PIV_ERROR` until `TC_PIV_link_init`.

## SELECT and the application property template

`TC_PIV_select` selects the PIV application by its complete AID or the TWIC
application by its 9-byte AID prefix (TWIC Part 2 v5 section 5.1, TWIC Part 3
v4 Appendix D.3), with Le `00`. SELECT is always plain (Part 2 section 4.2).
`TC_PIV_application_read` checks the answer, and `TC_PIV_select` uses it:

- The data field starts with one `61` template. DO `7F66` may follow it once
  with two nonzero `02` sizes, the card's largest command and response APDUs
  (ISO/IEC 7816-4 section 12.8.1). Each size is an unsigned big-endian count,
  since TWIC NEXGEN cards send 32769 as `02 02 80 01`. Other top-level DOs are
  skipped.
- Inside `61`: one `4F` with the expected AID prefix and two version bytes, one
  `79` holding a nonempty `4F`, and at most one `50`, `5F50` and `AC` (Part 2
  Tables 3 and 4). Part 2 section 3.1.1 and TWIC Part 2 v5 section 5.1.1
  require the complete AID in `4F`. An answer whose `4F` holds only the PIX
  `00 00 10 00 01 00`, with the RID in `79`, is `TC_PIV_INVALID`. A YubiKey
  answers in that form.
- In `AC` each `80` holds one algorithm identifier, `06 01 00` appears exactly
  once, and at most one of the secure messaging suites `27` and `2E` is listed
  (Part 2 Table 5). The suite is returned in `sm_suite`.
- PIV version `01 00` is `TC_PIV_CARD`. TWIC version `01 01` is
  `TC_TWIC_LEGACY_CARD` and `01 03` is `TC_TWIC_NEXGEN_CARD` (TWIC Part 2 v5
  section 4.1). TWIC Part 3 v4 Appendix D.3 states that any sub-version of
  version `01` is backward compatible with the Legacy data model and leaves the
  decision to the reader. Pass `TC_PIV_SELECT_TWIC_SUBVERSION_COMPATIBLE` to
  accept another sub-version as Legacy. Without the flag it is
  `TC_PIV_UNSUPPORTED`.

Selecting another application sets the card's security statuses to FALSE,
and reselecting the PIV application keeps them (Part 2 section 3.1.1). The link
clears its VCI and PIN status on every SELECT, so query the PIN again with
`TC_PIV_verify_status` when the application needs it. Selecting another
application also ends a bound secure messaging session, and reselecting the
PIV application keeps it. After a successful SELECT
the link records the application and profile. It applies the `7F66` limits to the channel
and sets the GET RESPONSE flags. The PIV application uses a plain CLA `00`
(Part 2 sections 4.2.6 and A.4.1). The TWIC application also requests `FF`
after `61 00` (TWIC Part 2 v5 section 5.2 note 3a, Appendix E). A failed SELECT
leaves no application selected, so select again before the next command.

## GET DATA

`TC_PIV_get_data` reads one data object by a tag of 1 to 3 bytes (Part 2
section 3.1.2). The answer to `9000` or `6282` must be exactly one TLV that
spans the data field. `out->encoded` is that TLV and `out->value` its value,
both borrowed from the response buffer. `out->status` records `6282`, the end
of the object before Le bytes (ISO/IEC 7816-4 Table 7).

| Answer                      | PIV application                     | TWIC application   |
| --------------------------- | ----------------------------------- | ------------------ |
| `53 L value`                | object, `TC_PIV_FORM_CONTAINER`     | same               |
| `53 00`                     | empty object (Part 1 section 4.1.1) | same               |
| `TAG L value`               | `7E` and `7F61` only                | any tag            |
| `TAG 00`                    | invalid                             | empty object       |
| `TAG 02 80 00`, constructed | invalid                             | empty object       |
| `9000` without data         | invalid                             | `TC_PIV_FORM_NONE` |

The TWIC rows come from TWIC Part 2 v5 sections 3.3.6 and 4.5. Another status
returns `TC_PIV_CARD_STATUS`, such as `6982` before the PIN, `6A81` for a
contactless denial or `6A82` for a missing object.

## VERIFY

`TC_PIV_verify_status` sends VERIFY without data (Part 2 section 3.2.1) for the
PIV PIN `80`, the Global PIN `00` or the pairing code `98`. `9000` reports the
reference verified. `63CX` reports X further tries. The pairing code has no
counter (Part 2 footnote 7), so `6300` reports neither. The link keeps one PIN
flag for `80` and `00`. `9000` sets it, and any other answer for either
reference clears it, since that reference is then FALSE on the card (Part 2
section 3.2.1.1).

`TC_PIV_pin_verify` verifies the PIN or Global PIN once:

1. It queries the counter. A verified reference returns `TC_PIV_OK` with
   `submitted` 0.
1. It sends the PIN only when the query reported at least `minimum_retries`
   tries. The floor is at least 2, so the last tries stay unspent. A lower count
   or an answer without a count returns `TC_PIV_REFUSED`.
1. The PIN is 6 to 8 ASCII digits, padded with `FF` to 8 bytes (Part 2 section
   2.4.3). The padded copy lives in a stack array and the command scratch, and
   both are wiped. A failed submission returns `TC_PIV_CARD_STATUS` with `63CX`,
   `6983` or `6A80` and is never retried.

The card rejects VERIFY for `80` and `00` outside the contact interface and the
VCI, and `98` on contactless without secure messaging (Part 2 section 3.2.1).
The library refuses those commands before sending, so a PIN never crosses the
contactless interface in plaintext (Part 1 Table 4). The VCI requires a
secured link, and a secured link sends GET DATA and VERIFY under
[secure messaging](piv-sm.md#secure-messaging-on-a-card-link). On contactless the card may
answer `6983` at an issuer-defined intermediate retry value. The TWIC
application defines no VERIFY, so both functions return `TC_PIV_UNSUPPORTED`
there.

## Status words

`TC_PIV_status_classify(sw, command, application, &retries)` maps a status word
to a `TC_PIV_status` by Part 1 section 5.6 Table 7. `6A88` means a missing key
or data reference, except on TWIC GET DATA, where it reports a missing object
(TWIC Part 2 v5 section 5.2). `63CX` on VERIFY writes X to `retries`.

## Card object readers

`TINY_CRYPTO_ENABLE_PIV_OBJECTS` adds readers for the objects that drive the
card session. Pass them the `encoded` span of a `TC_PIV_data_object`. They check
one complete object against its table, return views borrowed from the input,
charge no work and write their output only on `TC_TLV_OK`.

| Object                    | Tag      | Header                             | Reader                      |
| ------------------------- | -------- | ---------------------------------- | --------------------------- |
| Discovery Object          | `7E`     | `<tiny_crypto/piv_discovery.h>`    | `TC_PIV_discovery_read`     |
| Card Capability Container | `5FC107` | `<tiny_crypto/piv_card_objects.h>` | `TC_PIV_CCC_read`           |
| Key History               | `5FC10C` | `<tiny_crypto/piv_card_objects.h>` | `TC_PIV_key_history_read`   |
| BIT group template        | `7F61`   | `<tiny_crypto/piv_card_objects.h>` | `TC_PIV_bit_group_read`     |
| Pairing Code container    | `5FC123` | `<tiny_crypto/piv_card_objects.h>` | `TC_PIV_pairing_code_read`  |
| Certificate containers    | `5FC1xx` | `<tiny_crypto/piv_certificate.h>`  | `TC_PIV_certificate_decode` |

- Discovery (Part 1 section 3.3.2 and Table 1) must be exactly
  `7E 12 {4F 0B AID} {5F2F 02 xx yy}`. `TC_PIV_DISCOVERY_PIV` requires the PIV
  AID and a Table 1 policy. `TC_PIV_DISCOVERY_TWIC` reads a TWIC card, where
  the PIV application reports `40 00` or `04 00` and the TWIC application
  `00 00` under a TWIC AID (TWIC Part 2 v5 sections 4.2 and 4.7.5).
  `TC_PIV_discovery_pin_reference` returns `00` when the Global PIN is enabled
  and preferred, and `80` otherwise. Integrity comes from the Security Object
  or from reading the object under secure messaging. `TC_PIV_discovery_get`
  in `<tiny_crypto/piv_vci.h>` reads the object over the link and records in
  `secured` whether it arrived under secure messaging
  ([virtual contact interface](piv-sm.md#virtual-contact-interface)).
- The CCC, Key History and Pairing Code readers take `TC_PIV_CONTAINER` for the
  `53` object or `TC_PIV_CONTENTS` for its value. The CCC follows Part 1 Table
  9 and accepts the optional `E3` and `B4` elements of SP 800-73-4 Part 1 Table
  8 found on older cards. Key History checks the counts and the
  `http://<DNS name>/<SHA-256 hex>` URL of Part 1 section 3.3.3, at most 118
  bytes.
- The BIT group holds 0 to 2 finger templates of at most 28 bytes. A nonempty
  group requires the Discovery OCC bit (Part 1 section 3.3.6). Compare the two
  objects after reading both.
- The pairing code is PIN-gated secret material (Part 1 Table 2). The returned
  span borrows the response buffer, so wipe that buffer when done.
- `TC_PIV_certificate_decode` reads a certificate container and returns one
  DER certificate. A plain certificate borrows the container. A GZIP
  certificate, such as the SD 33 SMCS object with CertInfo `01`, is decoded into
  the caller's `der` buffer. Every failure after the argument checks wipes
  `der`. It needs `TINY_CRYPTO_ENABLE_GZIP`.

```c
#include <tiny_crypto/piv_card_objects.h>
#include <tiny_crypto/piv_certificate.h>
#include <tiny_crypto/piv_command.h>
#include <tiny_crypto/piv_discovery.h>

/* Choose the PIN reference from a Discovery Object and decode a certificate
 * object, both read with TC_PIV_get_data. On success, certificate borrows the
 * certificate object or der. */
int inspect_objects(const TC_PIV_data_object* discovery_object,
                    const TC_PIV_data_object* certificate_object, uint8_t* der,
                    size_t der_capacity, uint8_t* pin_reference, TC_bytes* certificate)
{
  static TC_GZIP_workspace gzip;
  TC_PIV_discovery discovery;
  TC_PIV_certificate container;
  size_t work = 200000; /* bounds the GZIP decoder */
  if (TC_PIV_discovery_read(discovery_object->encoded, TC_PIV_DISCOVERY_PIV, &discovery) !=
      TC_TLV_OK)
    return 0;
  *pin_reference = TC_PIV_discovery_pin_reference(&discovery);
  if (TC_PIV_certificate_decode(certificate_object->encoded, TC_PIV_CERTIFICATE_SLOT,
                                TC_PIV_CERTIFICATE_RECOMMENDED_BYTES, &gzip, &work,
                                (TC_buffer){der, der_capacity}, &container) != TC_TLV_OK)
    return 0; /* der is wiped */
  *certificate = container.certificate;
  return 1;
}
```

## Catalog and inventory

`TINY_CRYPTO_ENABLE_PIV_CATALOG` adds `<tiny_crypto/piv_catalog.h>`. It needs
the card commands only. `TC_PIV_catalog_count`, `TC_PIV_catalog_at` and
`TC_PIV_catalog_find` return constant `TC_PIV_object_info` entries: the tag,
container ID, object kind, access rule per interface, presence requirement,
key reference, capacity and the `TC_PIV_OBJECT_SECRET` flag.

| Application | Profile               | Catalog                                                  |
| ----------- | --------------------- | -------------------------------------------------------- |
| PIV         | `TC_PIV_CARD`         | 36 objects of SP 800-73-5 Part 1 Table 3, Tables 2 and 8 |
| TWIC        | `TC_TWIC_LEGACY_CARD` | `5FC102`, `5FC104`, `DFC101`, `DFC103`, `DFC10F`         |
| TWIC        | `TC_TWIC_NEXGEN_CARD` | the 12 readable objects of TWIC Part 2 v5 section 4.5    |

The Pairing Code container `5FC123` needs the PIN, and the VCI on
contactless (Part 1 Table 2). The TWIC Privacy Key container `DFC101` reads
on contact only (TWIC Part 2 v5 section 4.5). Both are secret. On the TWIC
application `DFC001`, `DFC002` and `DFC121` are optional and every other entry
is mandatory. Keys and the TWIC E-stickers are outside the catalogs.

`TC_PIV_inventory_read` reads the catalog of the selected application and
profile into one caller pool. It checks each rule against the link state
before sending anything: `PIN` needs a verified PIN, `VCI` needs the virtual
contact interface, and OCC is never available. Each object gets a state:

| State        | Meaning                                                                  |
| ------------ | ------------------------------------------------------------------------ |
| `PRESENT`    | read, with a nonempty value                                              |
| `EMPTY`      | `53 00`, a TWIC empty form, or a bare TWIC `9000` for an optional object |
| `ABSENT`     | `6A82`, or `6A88` on the TWIC application                                |
| `RESTRICTED` | the rule is unmet in the link state, and nothing was sent                |
| `DENIED`     | the rule was met, and the card answered `6982` or `6A81`                 |
| `OVERSIZED`  | the answer did not fit, and the plain link stayed usable                 |
| `SKIPPED`    | the pairing code without `TC_PIV_INVENTORY_PAIRING_CODE`                 |

Any other outcome aborts the read and wipes the pool bytes offered to the card
and the object array. That covers malformed answers, another card status,
transport failures, an exhausted exchange budget and any secure messaging
failure. Under secure messaging an answer that does not fit ends the session,
so it aborts too.

Set `objects` and `capacity` before the call. The array needs
`TC_PIV_catalog_count` entries, and a smaller array returns `TC_PIV_LIMIT`
before anything is sent. Answers arrive at the next free pool byte. A secured
read decrypts in place, so the value stays behind its secure messaging header.
`max_object_bytes` in the plan caps one object at
`TC_PIV_RESPONSE_BYTES(max_object_bytes)` pool bytes. A read goes out only when
its region can take one full answer of Ne bytes, or 256 bytes under secure
messaging, bounded by the card's DO `7F66` response limit. A smaller region is
`OVERSIZED`. `TC_PIV_INVENTORY_POOL_BYTES`
covers every PIV object at its Table 8 capacity. The capacities are floors, so
treat it as a starting point. SD 33 card 2 needs about 25 KiB. Work costs one
unit per catalog entry and one per kept pool byte.

The entries borrow the pool until `TC_PIV_inventory_clear`, which wipes the
used bytes and the array. The pool holds PIN-gated data and possibly the
pairing code or the TWIC Privacy Key, so clear it on every exit path. The
inventory records the link state it used in `link`.

```c
#include <tiny_crypto/piv_catalog.h>

/* Read the catalog of the application selected on link. objects holds
 * TC_PIV_CATALOG_PIV_OBJECTS entries. On TC_PIV_OK *restricted counts the
 * objects to read again after the PIN or the VCI, and the caller clears the
 * inventory when done with it. */
TC_PIV_result read_inventory(TC_PIV_link* link, TC_PIV_object* objects, uint8_t* pool,
                             size_t pool_capacity, TC_PIV_inventory* inventory,
                             size_t* restricted)
{
  /* Leave the pairing code out and cap one object at 16 KiB. */
  const TC_PIV_inventory_plan plan = {0, 16384};
  size_t work = TC_PIV_CATALOG_PIV_OBJECTS + pool_capacity;
  inventory->objects = objects;
  inventory->capacity = TC_PIV_CATALOG_PIV_OBJECTS;
  const TC_PIV_result result =
      TC_PIV_inventory_read(link, &plan, (TC_buffer){pool, pool_capacity}, &work, inventory);
  if (result != TC_PIV_OK)
    return result; /* an abort wiped the pool and the objects */
  *restricted = 0;
  for (size_t i = 0; i < inventory->count; ++i)
    *restricted += inventory->objects[i].state == TC_PIV_OBJECT_RESTRICTED;
  return TC_PIV_OK;
}
```

## Key proofs

`TINY_CRYPTO_ENABLE_PIV_KEY_PROOF` adds `<tiny_crypto/piv_key_proof.h>`. It needs
the card commands, key challenges and the X.509 reader. `TC_PIV_key_prove` has
a card key sign a fresh challenge and verifies the signature under the key of
its validated certificate. Validate the certificate path, revocation and
identifiers first. The proof checks the card's possession of the key only.

`TC_PIV_key_parameters_select` applies the key policy of `TC_PIV_key_policy`
before anything is sent or drawn:

| Key                | Identifier (SP 800-78-5 Table 9) | Profiles                                       |
| ------------------ | -------------------------------- | ---------------------------------------------- |
| RSA-2048           | `07`                             | all, and `TC_PIV_CARD` only through 2030-12-31 |
| RSA-3072           | `05`                             | `TC_PIV_CARD` and `TC_TWIC_LEGACY_CARD`        |
| RSA-1024           | `06`                             | `TC_TWIC_LEGACY_CARD` with `allow_rsa1024`     |
| P-256 with SHA-256 | `11`                             | `TC_PIV_CARD` and `TC_TWIC_LEGACY_CARD`        |
| P-384 with SHA-384 | `14`                             | `TC_PIV_CARD` and `TC_TWIC_LEGACY_CARD`        |

The certificate must assert digitalSignature in keyUsage, and an RSA key needs
an exponent of 65537 to 2^256 - 1 (SP 800-78-5 section 3.1). The Table 10 end
of RSA-2048 uses the policy time. TWIC profiles follow the TWIC reader policy,
and `TC_TWIC_NEXGEN_CARD` accepts only the RSA-2048 key `9E07` (TWIC Part 2 v5
sections 4.5 and 5.3). RSA challenges use SHA-256 with PKCS #1 v1.5, or PSS
with MGF1 and a 32-byte salt.

The command is GENERAL AUTHENTICATE with `7C {82 00, 81 L challenge}` in the
order of SP 800-73-5 Part 2 Appendix A.4.1 and the Le of the link, `00` on a
SHORT link. The channel chains a template above 255 bytes, and a secured link
sends it under secure messaging as `1C` fragments. The answer must be exactly
`7C {82 L signature}`.

| Key  | Application  | Contact    | Contactless        |
| ---- | ------------ | ---------- | ------------------ |
| `9A` | PIV          | PIN        | VCI and PIN        |
| `9C` | PIV          | PIN Always | VCI and PIN Always |
| `9E` | PIV and TWIC | Always     | Always             |

The link refuses `9A` and `9C` on contactless without the VCI before sending
(Part 1 Table 5). The card enforces the PIN and answers `6982`, returned as
`TC_PIV_CARD_STATUS`. `9C` is PIN Always, so verify the PIN immediately before
its proof. `TC_PIV_pin_verify` submits the PIN only while the card reports it
unverified, so prove `9C` directly after the PIN submission of the card session.
The TWIC application proves only `9E` under the profile of its SELECT, and only
on NEXGEN cards (TWIC Part 2 v5 section 5.3). The Legacy TWIC application
returns `TC_PIV_UNSUPPORTED` before sending. TWIC Part 2 v5 section 5.3 names
`9A` in its prose and `9E` in its command syntax. The library follows the syntax
table and TWIC Part 3 v4 section 4.4.4. A TWIC Legacy card proves its card key
on its PIV application, which reports `TC_PIV_CARD`, so the request names
`TC_TWIC_LEGACY_CARD`. The key management key, retired keys and symmetric keys
are outside this module.

`TC_PIV_key_proof_workspace` holds the challenge, the request and the answer.
Argument errors, refusals, the Legacy TWIC application and a provider without
`verify_digest` leave it unchanged, and every other return wipes it. The
certificate, its key bytes, the request and the provider must lie outside the
link and its scratch buffers, since every exchange rewrites them. Work covers
the challenge and the signature verification. The exchange budget covers the
chain fragments and the GET RESPONSE steps.

```c
#include <tiny_crypto/piv_key_proof.h>

/* Prove the card authentication key 9E on the application selected on link.
 * certificate is the validated certificate of container 5FC101 and now the
 * policy time. On TC_PIV_CARD_STATUS the card refused, and
 * TC_PIV_link_status(link) holds its answer. */
TC_PIV_result prove_card_authentication(TC_PIV_link* link, const TC_X509_certificate* certificate,
                                        const TC_X509_time* now, TC_random_source random,
                                        const TC_X509_signature_provider* provider,
                                        TC_PIV_key_proof_workspace* workspace)
{
  const TC_PIV_key_proof_request request = {
      certificate, {TC_PIV_CARD, *now, TC_PIV_RSA_PKCS1_V15, 0}, TC_PIV_KEY_CARD_AUTHENTICATION};
  TC_work_budget work = {1000000};
  /* Only TC_PIV_OK proves possession. Every return after the argument and
   * state checks wipes the workspace. */
  return TC_PIV_key_prove(link, &request, random, provider, workspace, &work);
}
```

`tiny_crypto::piv_key_prove` in `<tiny_crypto/piv_key_proof.hpp>` takes a
`PIVLink`.

## Example

```c
#include <tiny_crypto/piv_command.h>

/* Select the PIV application and read the CHUID container 5FC102 (SP 800-73-5
 * Part 1 Table 3). On TC_PIV_OK, chuid borrows response. On
 * TC_PIV_CARD_STATUS, *status holds the card's answer. */
TC_PIV_result read_chuid(TC_APDU_transport transport, TC_PIV_interface interface,
                         uint8_t* response, size_t capacity, TC_bytes* chuid, uint16_t* status)
{
  static const uint8_t tag[] = {0x5f, 0xc1, 0x02};
  uint8_t scratch[TC_APDU_SHORT_COMMAND_MAX_BYTES];
  /* SELECT, GET DATA and their GET RESPONSE steps share the budget. */
  const TC_PIV_link_options options = {{TC_APDU_SHORT, 0, 32, 0, 0}, interface, 0};
  TC_PIV_link link;
  TC_PIV_application application;
  TC_PIV_data_object object;
  TC_PIV_result result =
      TC_PIV_link_init(&link, transport, &options, (TC_buffer){scratch, sizeof scratch});
  if (result != TC_PIV_OK)
    return result;
  result = TC_PIV_select(&link, TC_PIV_APPLICATION_PIV, 0, (TC_buffer){response, capacity},
                         &application);
  if (result == TC_PIV_OK)
    result = TC_PIV_get_data(&link, (TC_bytes){tag, sizeof tag},
                             (TC_buffer){response, capacity}, &object);
  if (result == TC_PIV_OK)
    *chuid = object.value;
  *status = TC_PIV_link_status(&link);
  /* Every failure after the argument checks wiped the response buffer. */
  TC_PIV_link_clear(&link);
  return result;
}
```

The C++11 class `tiny_crypto::PIVLink` in `<tiny_crypto/piv_command.hpp>` owns a
link and clears it on destruction. `piv_application_read` and
`piv_status_classify` wrap the free functions. `tiny_crypto::PIVInventory` in
`<tiny_crypto/piv_catalog.hpp>` clears its inventory on destruction.

## Limits

- The template reader accepts at most 4096 bytes, 64 elements and 4 nesting
  levels. The card object readers accept one level and at most 16 elements.
- The card object readers check structure only. Authenticate the objects with
  the Security Object before relying on them.
- The card commands travel in plaintext until
  [secure messaging](piv-sm.md#secure-messaging-on-a-card-link) secures the
  link. CHANGE REFERENCE DATA, RESET RETRY COUNTER, PUT DATA, GENERATE
  ASYMMETRIC KEY PAIR and OCC VERIFY (`96`, `97`) are outside the library.
- The response buffer holds the whole answer and SW1 SW2.
  `TC_PIV_RESPONSE_BYTES(nr)` sizes a buffer for nr data bytes on any link.
- Two TWIC NEXGEN behaviours follow the specification text and await
  confirmation on a NEXGEN card: the `9E` key of TWIC Part 2 v5 section 5.3,
  and a TWIC AID in the Discovery Object of the PIV application (section
  4.7.5).

## Resource use

The card commands keep no mutable static state and allocate nothing. The
caller owns the link, the command scratch and every response buffer.
`sizeof(TC_PIV_link)` is 40 bytes on AVR, and a SHORT link needs 261 bytes of
command scratch. The `apdu_piv_read` profile of `tests/budgets/avr.json`
measures a plain SELECT, a GET DATA of the CHUID and a VERIFY query on an
ATmega2560 with avr-gcc 7.3.0 at `-Os`. Its static RAM counts the link, the
command scratch and a response buffer for a CHUID at its Part 1 Table 8
capacity of 2881 bytes:

| Resource   | Budget      | Largest parts                                          |
| ---------- | ----------- | ------------------------------------------------------ |
| Flash      | 13500 bytes | template reader, channel, TLV reader, GET DATA         |
| Static RAM | 3400 bytes  | 2900-byte CHUID response, scratch, link and AID tables |
| Stack      | 560 bytes   | SELECT through the template reader and TLV walk        |

The catalog, the inventory and the key proofs are larger and target
ESP32-class and desktop devices. They build for AVR in the compile checks
([AVR builds and budgets](testing.md#avr-builds-and-budgets)).
