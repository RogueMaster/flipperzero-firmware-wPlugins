# ZeroFIDO iPhone NFC Transport Writeup

Use this when you touch the NFC transport. The iPhone path depends on a few
ISO-DEP behaviors, and small cleanups can break Safari before WebKit
reaches CTAP.

We tested ZeroFIDO over NFC on iPhone. Android has a reader profile hook, but
the active profile matches the iPhone path until we test Android with real
devices and conformance tools.

## Transport Shape

ZeroFIDO keeps ISO-DEP inside the app:

```text
Flipper NFC listener
  -> ZeroFIDO RATS / ATS handling
  -> ZeroFIDO ISO-DEP I/R/S block handling
  -> ISO7816 APDU parser
  -> FIDO NFC dispatcher
  -> CTAP / U2F core
```

APDU code returns APDU payloads and status words. ISO-DEP code turns those bytes
into I-blocks, toggles block numbers, and handles reader ACK/NAK recovery. Do
not push ISO-DEP repair into CTAP or U2F handlers.

This is a small card-side ISO-DEP transport for the ZeroFIDO authenticator
surface, not a general ISO14443-4 stack.

## Activation

The listener advertises an ISO14443-A card with stable identity fields:

```text
UID  04 A1 B2 C3 D4 E5 F6
ATQA 44 00
SAK  20
ATS  05 78 91 E8 00
```

ZeroFIDO handles `RATS` in-app. When iPhone sends `E0 80`, the transport resets
the activation state, sends the ATS, clears stale chain/replay state, and starts
a fresh ISO-DEP exchange.

iPhone may probe the tag before WebKit selects the FIDO applet. Keep those
probes isolated from FIDO state:

- Native DESFire `60` / `AF` gets the compatibility version shim.
- Wrapped DESFire `90 60` / `90 AF` gets APDU-style DESFire version data.
- Type 4 NDEF SELECT gets `6A82`.
- Probe responses must preserve cached replayable FIDO I-blocks.
- Short probe loops after a large successful response may sleep for the cooldown
  window, but a new FIDO SELECT must work.

## FIDO SELECT

WebKit starts by selecting the CTAP NFC applet:

```text
00 A4 04 00 08 A0 00 00 06 47 2F 00 01
```

ZeroFIDO accepts:

- `P2=00`
- `P2=0C`
- canonical `Le=0`
- the legacy nine-byte AID plus trailing zero form

Normal builds answer:

```text
55 32 46 5F 56 32 90 00    # "U2F_V2" + 9000
```

FIDO2-only builds answer `FIDO_2_0 9000`. A legacy pre-select U2F VERSION APDU
can also select the FIDO surface and return `U2F_V2 9000`.

Each successful SELECT starts a new logical NFC session and clears pending
request, response, command-chain, transmit-chain, and replay state.

## CTAP2

CTAP2 uses NFC CTAP MSG:

```text
CLA = 80
INS = 10
P1  = 00, or 80 when the reader advertises NFCCTAP_GETRESPONSE
P2  = 00
```

ZeroFIDO handles `authenticatorGetInfo` on the callback path. The response
advertises NFC transport and suppresses USB transport, regardless of the build's
USB capability.

ZeroFIDO supports two response paths:

- **Direct no-GET_RESPONSE.** Safari uses this path. ZeroFIDO runs CTAP2 in the
  callback and returns `CTAP status byte + CBOR + 9000`. Large extended-APDU
  responses continue with ISO-DEP response chaining.
- **GET_RESPONSE-aware.** Readers set `P1=80` when they support
  NFCCTAP_GETRESPONSE. ZeroFIDO queues the request, returns `01 9100` while the
  worker runs, then serves the result through `80 11` or ISO GET RESPONSE
  (`00/80 C0`).

Large CTAP requests may use APDU command chaining. The transport appends chained
`80 10` fragments in the shared arena, ACKs each fragment, rejects duplicate or
stalled fragments, and dispatches CTAP only after the final fragment arrives.

`80 12 01 00` means NFCCTAP_CONTROL END. ZeroFIDO cancels pending work, clears
the selected applet, and resets exchange state.

## U2F

U2F APDUs use `CLA=00` and go through the same U2F adapter as USB HID MSG.
Validation failures that do not need user presence return from the NFC callback.
That keeps conformance negative tests out of the approval path.

Large U2F responses use ISO GET RESPONSE paging. U2F keeps its status word at
the end of the U2F response buffer; CTAP2 uses APDU `9000` for success.

## Recovery

iPhone recovery depends on byte-for-byte replay:

- Cache only ISO-DEP I-block responses.
- Replay the cached I-block for one-byte R-NAK frames such as `B2` / `B3`.
- Replay the cached I-block for normal R-NAK blocks.
- Advance an active transmit chain on R-ACK.
- Clear completed-chain state on the terminal R-ACK without sending a new frame.
- Replay the cached response for empty I-blocks, or ACK when no replay exists.
- ACK PPS-like frames only when the ACK would not replace a large cached FIDO
  response that a later R-NAK needs.
- Ignore CID-bearing blocks unless ATS advertised CID. The current ATS does not
  advertise CID.
- On field-off, halt, or S-DESELECT, cancel UI work, clear replay/chaining
  state, and reset the ISO-DEP PCB.

## iPhone Flow

A Safari registration or assertion follows this shape:

```text
1. iPhone polls NFC-A.
2. iPhone sends RATS:
   E0 80
3. ZeroFIDO sends ATS:
   05 78 91 E8 00
4. iPhone may send DESFire or NDEF probes.
5. ZeroFIDO answers DESFire and rejects NDEF without selecting FIDO.
6. WebKit selects the FIDO applet:
   00 A4 04 00 08 A0 00 00 06 47 2F 00 01
7. ZeroFIDO returns:
   U2F_V2 9000
8. WebKit sends CTAP2 getInfo as extended APDU:
   80 10 00 00 00 00 01 04 00 00
9. ZeroFIDO returns:
   CTAP status byte + getInfo CBOR + 9000
10. WebKit sends makeCredential or getAssertion through CTAP2.
11. ZeroFIDO returns:
   CTAP status byte + CTAP response CBOR + 9000
12. Large responses continue with ISO-DEP response chaining.
13. R-ACK advances the chain. R-NAK replays the previous I-block.
```

Safari needs the direct path. WebKit's public NFC CTAP driver has a FIXME for
NFCCTAP_GETRESPONSE and sends CTAP2 as `CLA=80, INS=10`, expecting APDU status
`9000` on success.

## Tests

The native transport regression harness covers:

- listener profile and ATS shape
- RATS and repeated activation reset
- FIDO SELECT variants and FIDO2-only SELECT response
- DESFire native and wrapped probe handling
- NDEF rejection
- pre-select rejection and U2F VERSION fallback
- WebKit extended CTAP2 getInfo
- direct no-GET_RESPONSE CTAP2
- GET_RESPONSE-aware queued CTAP2
- ISO GET RESPONSE and NFCCTAP_GETRESPONSE paging
- APDU command chaining, duplicate fragments, and stalled fragments
- large response ISO-DEP chaining
- R-NAK replay, R-ACK chain advancement, empty I-block recovery, and late PPS
  handling
- field-off, halt, S-DESELECT, and stale worker completion cleanup

Diagnostic builds emit matching NFC trace breadcrumbs. Release builds omit the
trace implementation by default.

## Do Not Break

Treat these as compatibility rules:

- Keep the ATS profile and response chunk sizes unless you retest iPhone.
- Do not let DESFire or NDEF probes select or reset the FIDO applet.
- Do not let probe responses overwrite a replayable FIDO I-block.
- Keep Safari working without APDU-level GET RESPONSE.
- Use ISO-DEP response chaining for large direct extended-APDU CTAP2 responses.
- Replay the previous I-block byte-for-byte on R-NAK.
- Keep ISO-DEP recovery in the NFC transport.
