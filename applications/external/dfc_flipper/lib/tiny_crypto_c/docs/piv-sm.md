<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# PIV secure messaging

The library implements the client application side of PIV secure messaging
from SP 800-73-5 Part 2 section 4 for cipher suites CS2 and CS7, in two
layers:

- `<tiny_crypto/piv_sm.h>` is the session: the ephemeral key pair, ECDH,
  session-key derivation, key confirmation, the encryption counter, the MAC
  chaining values and padding (sections 4.1 and 4.2).
- `<tiny_crypto/piv_sm_apdu.h>` puts the session on a
  [PIV card link](piv-card.md): the key establishment command, the
  `87/97/99/8E` wire format of protected commands and responses, command
  chaining and the session-loss rule (sections 4.1.8 and 4.2 to 4.3). GET DATA
  and VERIFY on a secured link are protected with no change to their calls.
- `<tiny_crypto/piv_vci.h>` establishes the
  [virtual contact interface](#virtual-contact-interface) on a secured link
  (Part 1 section 5.5).

The application owns I/O, the trust decision for the content signer and the
choice of when to secure the link.

## Build configuration

Enable `TINY_CRYPTO_ENABLE_PIV_SM`. The desktop profile enables it with its
dependencies. The session module needs AES with `TINY_CRYPTO_AES_ENABLE_DYNAMIC`,
SHA-256, SSKDF and EC. `TC_PIV_SM_authenticate_response` also needs X.509 and
PIV CVC parsing. `TINY_CRYPTO_ENABLE_PIV_SM_APDU` adds the link layer and
needs the PIV card commands, the session and PIV CVC parsing.
`TINY_CRYPTO_ENABLE_PIV_VCI` adds the virtual contact interface and needs the
link layer and the PIV object readers. The suites
follow Table 18.

| Suite | P1   | Curve | KDF hash | Session keys | Nonce    |
| ----- | ---- | ----- | -------- | ------------ | -------- |
| CS2   | `27` | P-256 | SHA-256  | AES-128      | 16 bytes |
| CS7   | `2E` | P-384 | SHA-384  | AES-256      | 24 bytes |

`TINY_CRYPTO_PIV_SM_ENABLE_CS2` and `TINY_CRYPTO_PIV_SM_ENABLE_CS7` select the suites. Both
are ON in every profile. CS2 needs P-256. CS7 needs P-384 and SHA-384. Disable
an unused curve separately with `TINY_CRYPTO_EC_ENABLE_P256` or
`TINY_CRYPTO_EC_ENABLE_P384`. `TC_PIV_SM_KEY_BYTES` and `TC_PIV_SM_COORDINATE_BYTES`
size the session for the largest enabled suite.

## Objects and storage

| Object                | Owner  | Lifetime                                              |
| --------------------- | ------ | ----------------------------------------------------- |
| `TC_PIV_SM`           | caller | One card session. Zero-initialize before first use.   |
| `TC_PIV_SM_workspace` | caller | One call. Processed calls wipe it before returning.   |
| `TC_PIV_SM_handshake` | caller | Borrows session storage until the session changes.    |
| `TC_PIV_SM_peer`      | caller | Borrows the received response for `TC_PIV_SM_finish`. |

On a link, the link borrows the session from `TC_PIV_SM_key_request`, and the
workspace and a secure messaging scratch buffer from `TC_PIV_link_secure`,
until the session is unbound. Keep all three alive and unshared while the
link holds them.

Treat `TC_PIV_SM` members as private. Read the state with `TC_PIV_SM_get_state`.
Never copy a live session, because the copy would reuse keys and counters.
Sessions retain no input pointers. Every call that passes argument validation
wipes the workspace before returning, so one workspace can serve other
operations between calls. Keep every writable object disjoint from the inputs.
`TC_PIV_SM_protect` is the one exception: its ciphertext output may lie inside
the authenticated spans.

The session moves through four states.

| State                    | Meaning           | Accepted calls                      |
| ------------------------ | ----------------- | ----------------------------------- |
| `TC_PIV_SM_IDLE`         | No session        | `begin`, `clear`                    |
| `TC_PIV_SM_ESTABLISHING` | `begin` succeeded | `finish` or `authenticate_response` |
| `TC_PIV_SM_READY`        | Keys established  | `protect`                           |
| `TC_PIV_SM_PENDING`      | One command sent  | `unprotect`                         |

`TC_PIV_SM_begin` and `TC_PIV_SM_clear` are accepted in any state and discard
the previous session. Call `TC_PIV_SM_clear` when the card is removed or when
command delivery becomes uncertain.

## Secure messaging on a card link

The flow follows section 4.1.1 on a link that selected the PIV application:

1. Read the Secure Messaging Certificate Signer `5FC122` and the CHUID `5FC102`
   in plaintext, both readable on either interface (Part 1 Table 2). Decode the
   content-signing certificate with `TC_PIV_certificate_decode` and validate
   its path, usage, policy, time and revocation status.
1. `TC_PIV_SM_key_request` starts the session with the suite from the
   application property template, sends GENERAL AUTHENTICATE with CLA `00`,
   INS `87`, P1 set to the suite and P2 `04`, and decodes the answer into a
   `TC_PIV_SM_peer`. The session is bound to the link.
1. `TC_PIV_SM_authenticate_response` verifies the card CVC under the content
   signer, binds the card UUID from the CHUID and completes key confirmation.
1. `TC_PIV_link_secure` protects the commands that follow. SELECT and GET
   RESPONSE stay plain.
1. `TC_PIV_link_unsecure`, `TC_PIV_link_clear` or a SELECT of another
   application clears the session.

```c
#include <tiny_crypto/piv_sm_apdu.h>
#include <tiny_crypto/piv_sm_authenticate.h>

/* The link borrows the session, workspace and scratch while it is secured. */
typedef struct {
  TC_PIV_SM session;
  TC_PIV_SM_workspace workspace;
  uint8_t sm_scratch[TC_PIV_SM_COMMAND_DATA_BYTES(TC_PIV_COMMAND_MAX_NC)];
  uint8_t response[TC_PIV_SM_KEY_RESPONSE_BYTES];
} secure_storage;

/* Secure a link that selected the PIV application. signer is the validated
 * content-signing certificate from 5FC122 and card_uuid the CHUID GUID. */
TC_PIV_result secure_link(TC_PIV_link* link, const TC_PIV_application* application,
                          secure_storage* storage, TC_random_source random,
                          const TC_X509_certificate* signer, TC_bytes card_uuid,
                          const TC_X509_signature_provider* signatures)
{
  static const uint8_t host_id[8] = {0};
  const TC_TLV_limits limits = {4096, 4096, 64, 8};
  TC_PIV_SM_peer peer;
  if (!application->sm_suite)
    return TC_PIV_UNSUPPORTED; /* the card offers no secure messaging */
  TC_PIV_result result = TC_PIV_SM_key_request(
      link, &storage->session, (TC_PIV_SM_suite)application->sm_suite, host_id, random,
      (TC_buffer){storage->response, sizeof storage->response}, &peer, &storage->workspace);
  if (result != TC_PIV_OK)
    return result; /* the session is IDLE */
  const TC_PIV_SM_authentication authentication = {peer,   {NULL, 0}, card_uuid,
                                                   signer, &limits,   signatures};
  TC_PIV_SM_authentication_workspace verification;
  size_t work = 2000000; /* bounds the CVC signature check */
  if (TC_PIV_SM_authenticate_response(&storage->session, &authentication, &work,
                                      &verification) != TC_CREDENTIAL_VALID) {
    TC_PIV_link_unsecure(link); /* unbinds the IDLE session */
    return TC_PIV_INVALID;
  }
  return TC_PIV_link_secure(link, &storage->workspace,
                            (TC_buffer){storage->sm_scratch, sizeof storage->sm_scratch});
}
```

The key establishment answer needs `TC_PIV_SM_KEY_RESPONSE_BYTES` (326) of
response buffer, the size of the CS7 answer with the largest card CVC of Table
19\. `TC_PIV_SM_key_request` refuses a link that is secured or has lost its session
and returns `TC_PIV_UNSUPPORTED` for a suite the build lacks or the card did
not announce. Those results change nothing. Every later failure clears the
session.

### Protected command and response format

A protected command carries CLA `0C` and the SM data field
`[87 L 01 ciphertext] [97 01 00] 8E 08 MAC` with a one-byte Le `00`
(sections 4.2.3 and 4.2.4, footnote 22). `87` is present when the command has
data, and `97 01 00` when the plain command has Le. The C-MAC covers the MAC
chaining value, the header block `0C INS P1 P2 80 00 .. 00` and the `87` and
`97` objects. An SM data field above 255 bytes goes out as `1C` fragments of
255 bytes and a final `0C` fragment. Every link sends secure messaging with
SHORT length fields, since footnote 22 fixes a one-byte Le and ISO/IEC 7816-4
5.2 never mixes short and extended fields. `TC_PIV_SM_COMMAND_DATA_BYTES(nc)`
bounds the SM data field for `nc` plain bytes. `TC_PIV_SM_MAX_PLAIN_NC`
(65503) is the largest plain data field.

The answer must be `[87 L 01 ciphertext] 99 02 SW 8E 08 MAC` with nothing after
it (sections 4.2.5 and 4.2.6). The channel collects `61XX` chunks with plain
GET RESPONSE `00 C0 00 00 XX`, which leaves the counter unchanged (4.2.2). The
link checks the R-MAC before it decrypts, and the plaintext replaces the
ciphertext in the caller's response buffer, so no second buffer is needed.
GET DATA frames the decrypted object in place and returns spans into the
response buffer. The status inside `99` becomes the command status and
`TC_PIV_link_status`. An inner status other than `9000`, such as `6982` before
the PIN, returns `TC_PIV_CARD_STATUS` and keeps the session.

### Session loss

Section 4.3 and footnote 25 end the session on any secure messaging error.
Once a command is protected, the link ends the session on every other
outcome:

| Outcome                                                                                              | Result               | `TC_PIV_link_status` |
| ---------------------------------------------------------------------------------------------------- | -------------------- | -------------------- |
| Outer status other than `9000`, such as `6882`, `6987`, `6988`, `6CXX`, or `6883` on a `1C` fragment | `TC_PIV_CARD_STATUS` | the outer status     |
| Malformed SM objects, `87` on a VERIFY answer, a failed R-MAC or padding                             | `TC_PIV_INVALID`     | 0                    |
| Response capacity or exchange budget exhausted during the exchange                                   | `TC_PIV_LIMIT`       | 0                    |
| Transport failure, or a protect failure such as an exhausted counter                                 | `TC_PIV_ERROR`       | 0                    |

A transport failure on a plain command, such as SELECT, ends a bound session
the same way, since the stopped link cannot continue it.

The session is cleared, the VCI and PIN status are cleared, the response
buffer and the secure messaging scratch are wiped, and `TC_PIV_link_info_get`
reports `sm_lost`. The link then refuses GET DATA, VERIFY and GENERAL
AUTHENTICATE with `TC_PIV_REFUSED` and never falls back to plaintext. Call
`TC_PIV_link_unsecure` to continue in plaintext or to establish a new session.
A command that needs more scratch, more channel scratch or more budget than
remains returns `TC_PIV_LIMIT` before it is protected, and the session stays
READY.

On a contactless link a PIN reaches the card only under secure messaging with
the VCI, and VERIFY and GET DATA on a secured link are always protected
(Part 1 Table 4, Part 2 section 3.2.1).

## Virtual contact interface

Over contactless, the PIV PIN, the PIV Authentication key and the objects
marked VCI in Part 1 Table 2 need the virtual contact interface. Part 1
section 5.5 and Table 2 footnote 9 define it as a security condition: the
command travels under secure messaging, the Discovery Object is present with
policy bit 4 set, and either the pairing code was verified or policy bit 3
waives it.

1. Secure the link as above.
1. `TC_PIV_discovery_get` reads the Discovery Object `7E` with GET DATA. On a
   secured link the answer arrives under secure messaging, and the result
   records `secured = 1`. The object carries no signature of its own, so the
   VCI and PIN-reference decisions use this protected read (Part 1 section
   3.3.2). `TC_PIV_discovery_pin_reference` gives the PIN reference for
   VERIFY.
1. `TC_PIV_vci_establish` checks the policy. With bit 3 set nothing is sent,
   and the mode is `TC_PIV_VCI_WITHOUT_PAIRING`. Otherwise VERIFY `98` goes
   out under secure messaging with the 8-digit pairing code (Part 2 section
   3.2.1.3 and Table 25), and `9000` gives `TC_PIV_VCI_PAIRED`.
1. `TC_PIV_link_info_get` then reports `vci = 1`, and `TC_PIV_pin_verify` may
   send the PIN on the contactless link.

```c
#include <tiny_crypto/piv_vci.h>

/* Open the VCI on a secured link. pairing_code holds the 8 digits, or is
 * empty for a card whose policy waives pairing. discovery_response must stay
 * unchanged while discovery->aid is used. */
TC_PIV_result open_vci(TC_PIV_link* link, TC_bytes pairing_code, uint8_t* discovery_response,
                       size_t response_capacity, TC_PIV_discovery* discovery)
{
  TC_PIV_vci_mode mode;
  TC_PIV_result result =
      TC_PIV_discovery_get(link, TC_PIV_DISCOVERY_PIV,
                           (TC_buffer){discovery_response, response_capacity}, discovery);
  if (result != TC_PIV_OK)
    return result; /* CARD_STATUS 6A82: the card has no Discovery Object */
  /* TC_PIV_UNSUPPORTED: the card has no VCI. TC_PIV_CARD_STATUS with
   * TC_PIV_link_status 6300: a wrong pairing code, and the session stays
   * READY. */
  return TC_PIV_vci_establish(link, discovery, pairing_code, &mode);
}
```

`TC_PIV_DISCOVERY_RESPONSE_BYTES` sizes the response buffer for the Discovery
Object on any link. The results follow `<tiny_crypto/piv_vci.h>`:

| Result               | Meaning                                                                                            |
| -------------------- | -------------------------------------------------------------------------------------------------- |
| `TC_PIV_UNSUPPORTED` | the TWIC application or profile, or policy bit 4 clear                                             |
| `TC_PIV_REFUSED`     | an unsecured link, a lost session, or a Discovery Object read without secure messaging             |
| `TC_PIV_ARGUMENT`    | a code other than 8 ASCII digits, an empty code when pairing is required, or overlap with the link |
| `TC_PIV_CARD_STATUS` | a rejected code, such as `6300`, clears the VCI. An outer SM status also ends the session          |

The VCI ends with the conditions it depends on: a SELECT, `TC_PIV_link_unsecure`,
a session loss and a new key request all clear it. The pairing code is secret
material. The library copies it to a stack array and the secure messaging
scratch, and wipes both. The contact interface accepts the call too, where it
serves no purpose (Part 1 Table 4 footnote 11).

## Key establishment

The session steps map to the client steps in section 4.1.1.
`TC_PIV_SM_key_request` runs steps 1 to 3 below on a link.

1. `TC_PIV_SM_begin` sets CB_H to zero (H1), generates the ephemeral key pair
   for the selected suite (H2) and returns the host identifier and the
   uncompressed ephemeral public key in a `TC_PIV_SM_handshake`.
1. GENERAL AUTHENTICATE with CLA `00`, INS `87`, P1 set to the suite and P2
   `04` carries `7C { 81 { CB_H || ID_sH || Q_eH } 82 00 }` (section 4.1.8).
1. The answer `7C { 82 { CB_ICC || N_ICC || AuthCryptogram_ICC || C_ICC } }`
   is decoded into a `TC_PIV_SM_peer`. `certificate` holds the exact encoded CVC, because the
   derivation hashes those bytes into ID_sICC (H6). `card_control` holds the
   received CB_ICC byte.
1. The application verifies the CVC signature and the content-signing
   certificate through its trust workflow (H5).
1. `TC_PIV_SM_finish` takes the authenticated public key and the unchanged peer
   fields. It rejects a nonzero CB_ICC (H4), derives the session keys with the
   OtherInfo layout from section 4.1.6 (H6 to H11) and checks the key
   confirmation cryptogram from section 4.1.7 (H12). The ephemeral private key,
   shared secret and confirmation key are wiped (H9, H11, H13).

With X.509 and PIV CVC enabled, `TC_PIV_SM_authenticate_response` in
`<tiny_crypto/piv_sm_authenticate.h>` performs steps 4 and 5. The application
still validates the content-signing certificate's path, usage, policy, time and
revocation status first and passes it as `signer`. See
[PIV CVC verification](piv-cvc.md) for the chain rules.

```c
TC_PIV_SM session = {0};
TC_PIV_SM_workspace workspace;
TC_PIV_SM_handshake handshake;
TC_status status = TC_PIV_SM_begin(&session, TC_PIV_SM_CS2, host_id, random,
                                   &handshake, &workspace);
if (status != TC_OK)
    return status;
/* Encode handshake.host_identifier and handshake.public_key into
 * GENERAL AUTHENTICATE, exchange it and decode the 82 object. */
TC_PIV_SM_peer peer = {
    .certificate = cvc,
    .nonce = nonce,
    .cryptogram = cryptogram,
    .card_control = cb_icc
};
TC_PIV_SM_authentication authentication = {
    .peer = peer,
    .intermediate = {NULL, 0},
    .expected_uuid = card_uuid,
    .signer = &content_signer,
    .limits = &limits,
    .signatures = &signatures
};
size_t work = PIV_SM_WORK; /* application's X.509 work allowance */
TC_PIV_SM_authentication_workspace auth_workspace;
TC_credential_status accepted = TC_PIV_SM_authenticate_response(
    &session, &authentication, &work, &auth_workspace);
if (accepted != TC_CREDENTIAL_VALID) {
    /* Every failure after argument validation leaves the session IDLE. */
    return TC_ERROR;
}
```

`TC_PIV_SM_authenticate_response` returns `TC_credential_status`.
`TC_CREDENTIAL_VALID` leaves the session READY. A nonzero CB_ICC, a wrongly
sized nonce or cryptogram, a rejected CVC chain and a differing cryptogram
return `TC_CREDENTIAL_INVALID`. `TC_CREDENTIAL_UNSUPPORTED` and
`TC_CREDENTIAL_LIMIT` come from CVC parsing and verification, and `LIMIT`
includes an exhausted `work` counter. `TC_CREDENTIAL_ERROR` reports an argument
error, which leaves the session ESTABLISHING, or a signature provider or
key-derivation failure, which leaves it IDLE. Check `TC_PIV_SM_get_state` to
tell them apart.

`TC_PIV_SM_finish` returns `TC_MISMATCH` when the cryptogram differs and
`TC_ERROR` for the other failures listed in the header. Both leave the session
IDLE. Argument errors leave it ESTABLISHING and unchanged.

## Protected commands

The link layer builds these spans itself. This section and the next describe
the session calls for framing outside `<tiny_crypto/piv_sm_apdu.h>`.

Section 4.2.3 computes the command MAC over the MAC chaining value, a 16-byte
encoded header, the `87` object and the `97` object. The session supplies the
chaining value. The caller supplies the rest as ordered spans in
`TC_PIV_SM_protect_request.authenticated`, up to
`TC_PIV_SM_AUTHENTICATED_SPANS_MAX` spans.

- The encoded header is CLA `0C`, INS, P1, P2 and the padding `80 00 ... 00`.
- The `87` object header ends with the padding indicator `01`.
- The ciphertext output span must appear inside one authenticated span. The
  call writes the ciphertext before computing the MAC, so the spans can point
  into the output APDU.
- The `97 01 00` object follows when the plain command would carry Le.

`TC_PIV_SM_ciphertext_size` returns the padded length. Padding always adds 1 to
16 bytes, and empty plaintext has empty ciphertext. The call encrypts with
AES-CBC under the counter-derived IV from section 4.2.2, writes the ciphertext
length and the 8-byte tag, and moves the session to PENDING. The application
appends `8E 08 tag` and the new Le byte (section 4.2.4).

```c
uint8_t header[16] = {0x0c, ins, p1, p2, 0x80};
uint8_t object_header[] = {0x87, (uint8_t)(padded + 1), 0x01};
uint8_t le_object[] = {0x97, 0x01, 0x00};
const TC_bytes authenticated[] = {
    {header, sizeof header},
    {object_header, sizeof object_header},
    {ciphertext, padded},
    {le_object, sizeof le_object}
};
TC_PIV_SM_protect_request request = {
    .plaintext = command_data,
    .ciphertext = {ciphertext, padded},
    .authenticated = authenticated,
    .authenticated_count = 4
};
size_t ciphertext_length;
uint8_t tag[8];
if (TC_PIV_SM_protect(&session, &request, &ciphertext_length,
                      (TC_buffer){tag, sizeof tag},
                      &workspace) != TC_OK) {
    /* Argument errors leave the session unchanged. Other failures end it. */
    return TC_ERROR;
}
```

The example uses a one-byte `87` length, which covers ciphertext up to 112
bytes. Longer ciphertext needs the `81` or `82` length forms.

## Protected responses

Section 4.2.5 computes the response MAC over the chaining value, the `87`
object when present and the `99` status object. Pass those bytes as
authenticated spans, the ciphertext without the padding indicator, and the
value of the `8E` object as `tag`. The ciphertext span must lie inside an
authenticated span and its length must be a multiple of 16.

`TC_PIV_SM_unprotect` checks the tag before it decrypts, so plaintext is
released only after authentication. The status word inside the `99` object is
authenticated. The outer SW1-SW2 of the response APDU is transport status, and
the caller checks it before calling unprotect.

Pass the plaintext storage as a `TC_buffer`. It is disjoint from every input, or exactly
`request.ciphertext.data` with a capacity of at most
`request.ciphertext.length`. That exact alias decrypts in place: the final
block is decrypted first from intact ciphertext, and each earlier block is
read before its plaintext is written. Any other overlap is an argument error.
A padding failure is detected before any plaintext is written, so the
ciphertext stays unchanged.

| Result        | State   | Meaning                                                            |
| ------------- | ------- | ------------------------------------------------------------------ |
| `TC_OK`       | READY   | Plaintext and length written. The next command may follow.         |
| `TC_MISMATCH` | IDLE    | The response tag differs.                                          |
| `TC_ERROR`    | PENDING | Argument error or short plaintext buffer. Retry the same response. |
| `TC_ERROR`    | IDLE    | Malformed padding, exhausted counter or cipher failure.            |

A plaintext capacity of `request.ciphertext.length` always suffices. Check
`TC_PIV_SM_get_state` after `TC_ERROR` to tell a retryable call from one that
ended the session.

## Error handling

- Argument errors return `TC_ERROR` and leave the session, workspace and
  outputs unchanged. They include NULL pointers, overlapping storage, a
  ciphertext span outside every authenticated span and calls in the wrong
  state.
- After validation, peer, cryptographic, padding and counter failures clear the
  session. Written ciphertext or plaintext is wiped.
- Only one protected command may be pending. Protect the next command after its
  response has been unprotected.
- The encryption counter stops the session before the low 120 bits repeat.
  Establish a new session to continue.

Section 4.2.7 lists the card's secure messaging error status words. A card
error response carries no `8E` object. Clear the session and restart key
establishment.

## C++

`tiny_crypto::PIVSM` in `<tiny_crypto/piv_sm.hpp>` wraps one session. It clears
the session on destruction and cannot be copied or moved. Its member functions
match the C calls and take the same request types through references. Workspace
stays outside the object so it can be shared with other operations. `native()`
returns the `TC_PIV_SM` for the C layers.

`<tiny_crypto/piv_sm_apdu.hpp>` adds `piv_sm_key_request`, `piv_link_secure`
and `piv_link_unsecure` over a `PIVLink` and a `PIVSM`. The link holds a
pointer to the session while it is bound, so declare the `PIVSM` before the
`PIVLink`. The link is then destroyed first, and its destructor clears the
bound session while the session still exists. `<tiny_crypto/piv_vci.hpp>`
adds `piv_discovery_get` and `piv_vci_establish` over a `PIVLink`.

## Resource use

The framing adds no static state and allocates nothing. The link borrows the
session, the workspace and the secure messaging scratch from the caller. The
`piv_sm_cs2` profile of `tests/budgets/avr.json` measures CS2 key
establishment and a protected GET DATA of the CHUID on an ATmega2560 with
avr-gcc 7.3.0 at `-Os`
([AVR builds and budgets](testing.md#avr-builds-and-budgets)):

| Resource   | Budget      | Contents                                                       |
| ---------- | ----------- | -------------------------------------------------------------- |
| Flash      | 36700 bytes | EC P-256, AES, CMAC, SSKDF, the CVC reader and the PIV link    |
| Static RAM | 4700 bytes  | link, session, 1090-byte workspace, scratch and CHUID response |
| Stack      | 750 bytes   | key establishment through ECDH                                 |

The framing code of `piv_sm_apdu.c` and `piv_sm_key_request.c` takes at most
3800 bytes of that flash. The ATmega328P (32 KiB flash, 2 KiB RAM) cannot hold
secure messaging.

## Tests

`test_piv_sm` covers the suites, state transitions, `TC_PIV_SM_ciphertext_size`
limits, argument errors and in-place decryption. `test_piv_sm_apdu` drives the
link layer against the card model in `tests/support/sm_card.c`: the wire
format, `87` lengths of one to three octets, `1C` chaining, GET RESPONSE, the
key establishment framing and every session-loss case. `test_piv_sm_synthetic`
replays generated sessions, and `test_sm_primitives_corpus` replays the
recorded NIST SD 33 exchanges of `tests/vectors/piv/sm_captures` byte for byte
through the link. `test_piv_vci` covers the Discovery Object read on plain
and secured links, pairing, pairing-free policies, rejected codes, refusals and
the events that clear the VCI, and `test_cpp_piv_vci` runs the C++ wrappers.
`test_piv_sm_authenticate` checks CVC chains through the
combined helper. `fuzz_piv_apdu` sends raw and authenticated secure messaging answers
through the link and checks the session-loss rule. See [Running the tests](testing.md).
