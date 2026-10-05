# Architecture

The application is built in layers, and the rule that shapes all of them is
that the further down a layer sits, the less it is allowed to know. The
cryptography knows nothing about APDUs, the protocol knows nothing about the
Flipper, and neither knows that a user interface exists. That is what lets
`tests/host` compile the lower two thirds of the application natively and run
it against the published ICAO test vectors without a device in reach.

```
  scenes/            twenty three scenes, one file each
  views/             the two views the stock module set does not provide
     |
  worker/            the read state machine, on the NFC thread; the export
     |
  access/            the access control drivers and the registry that picks one
     |
  crypto/            BAC, PACE, Secure Messaging, key derivation, MAC, curves
  protocol/          TLV, APDU, the file catalogue, MRZ, LDS, security infos
     |
  transport/         one port: move an APDU, bring back the answer
     |
  sim/               the other thing behind that port: a chip with no radio
```

Nothing points upwards. `crypto` and `protocol` sit side by side because each
needs a little of the other - Secure Messaging wraps an APDU, a security info
names a cipher - and they are free of everything above.

## The layers

| Directory | What lives there |
| --- | --- |
| `protocol/` | `emrtd_tlv` (BER-TLV, no copies, no allocation), `emrtd_apdu` (ISO 7816-4 and the commands of Doc 9303), `emrtd_files` (the catalogue of elementary files), `emrtd_mrz` (check digits, the key input, parsing DG1), `emrtd_lds` (EF.COM, DG1, DG2, DG11, DG12, DG15, EF.SOD), `emrtd_security_info` (EF.CardAccess and DG14) |
| `crypto/` | `emrtd_crypto` (cipher properties), `emrtd_mac` (Retail MAC, AES-CMAC, ISO 9797-1 padding), `emrtd_kdf` (9303-11 section 9.7), `emrtd_ec` (the standardized curves), `emrtd_rng`, `emrtd_sm` (Secure Messaging), `emrtd_bac`, `emrtd_pace` |
| `transport/` | `emrtd_transceiver` (the vtable everything above calls, and the frame arithmetic), `emrtd_isodep` (ISO 14443-4 block transmission, ours rather than the firmware's), `emrtd_iso14443_4` (the two pollers behind the port) |
| `access/` | the driver interface, the registry, and one driver each for PACE and BAC |
| `worker/` | `emrtd_worker` (the read, on the NFC stack's thread) and `emrtd_export` (the SD card) |
| `views/` | `emrtd_read_view` (progress) and `emrtd_date_input` (a date as three fields) |
| `scenes/` | one file per scene, generated into an enum and a handler table by `emrtd_scene_config.h` |
| `sim/` | `emrtd_sim`, a passport chip simulated at the APDU level, behind the same port the radio is behind. Driven by the host suite only; the package does not compile it |

## The transceiver port

Every layer that needs the chip calls `emrtd_transceiver_exchange()`, which
is a vtable with a context pointer. On the device the implementation is
`transport/emrtd_iso14443_4.c`; otherwise it is `sim/emrtd_sim.c`, a chip
simulated down to the APDU, which answers BAC, PACE and Secure Messaging from
a small set of files of its own. Because the port is the only way down, the
same PACE run that opens a real passport can be exercised on a host with a
sanitizer attached.

The simulated chip has one caller, and that is the host suite, which drives it
directly. Everything above the port is the real thing there; only the radio is
absent. The released package does not compile `sim/` at all, and there is no
switch in the user interface that reaches it: a simulation is a way of testing
the application, never a feature of it.

The port also carries the two frame sizes - `fsc`, what the card announced in
its ATS, and `fsd`, what the reader can receive - because on this platform
those numbers decide how a file is read. See [protocol.md](protocol.md).

### Why there is a second port underneath it

On type A the implementation does not call the firmware's ISO 14443-4A poller.
It drives the ISO 14443-3A poller and runs the block transmission protocol
itself, in `transport/emrtd_isodep.c`, over a second and much smaller port:

```c
typedef EmrtdError (*EmrtdIsoDepFrameFn)(
    void* context, const uint8_t* tx, size_t tx_len,
    uint8_t* rx, size_t rx_cap, size_t* rx_len, uint32_t fwt_fc);
```

The last argument is the whole reason. `iso14443_4a_poller_send_block()` takes
no timeout, and the one it computes internally collapses to 120 microseconds
for any card whose ATS carries no TB1 - see items 10 to 12 of
[platform.md](platform.md). A passport given 120 microseconds to answer does
not answer, and the read fails in a way that looks exactly like a document
being moved away.

Splitting it at a frame function rather than at an APDU function is what makes
the layer testable: `tests/host/test_isodep.c` drives it against a simulated
chip that reassembles a chained command, chains its own answer, asks for
waiting time extensions and drops frames on request. The timings, the block
numbering and the recovery are all checked on the host, with sanitizers, and
none of it needs a Flipper.

Type B keeps the firmware's poller, whose waiting time comes from the ATQB and
is correct. The two paths meet again at `EmrtdTransceiver`, so nothing above
the transport knows which one is in use.

## Why the access drivers are a registry

A document opens with PACE or with BAC, and which one it wants is not
something the person holding it knows or should have to. Asking is the wrong
interface; guessing from the country is worse. So each protocol is a driver:

```c
struct EmrtdAccessDriver {
    const char* name;
    EmrtdAccessScore (*probe)(const uint8_t* card_access, size_t len,
                              const EmrtdCredentials* credentials,
                              EmrtdError* reason);
    EmrtdError (*authenticate)(EmrtdTransceiver* transceiver, ...,
                               EmrtdSm* out_session,
                               EmrtdAccessOutcome* out_outcome);
    bool reselect_application;
};
```

The worker reads `EF.CardAccess`, which is readable before any
authentication, asks every registered driver what it makes of it, and runs
them in order of the score they returned. A driver that says
`EmrtdAccessScoreAnnounced` has seen its own protocol in the file; one that
says `EmrtdAccessScorePossible` has no evidence either way, which is the
honest answer for BAC, because a chip does not advertise it. A driver that
cannot work says `EmrtdAccessScoreUnsupported` and puts an `EmrtdError` in
`reason`, and that is what the user is shown: not "authentication failed" but
"this chip asks for a curve this build cannot compute".

Three things follow from the shape:

- **Falling back is free.** If PACE is refused by the chip the worker moves
  to the next driver rather than to an error screen.
- **Adding a protocol does not touch the reader.** PACE with the integrated
  mapping, when someone implements it, is a new driver and one line in the
  registry. `emrtd_worker.c` does not change.
- **The user can override.** `EmrtdAccessMethodPace` and
  `EmrtdAccessMethodBac` pin the choice to one driver, which is what makes a
  misbehaving document diagnosable.

`reselect_application` is in the interface because BAC leaves the application
selected and PACE does not; the worker asks rather than assuming.

## Threads

Two threads matter, and the boundary between them is the single most common
source of hangs in a Flipper application.

The **GUI thread** runs the scene manager and every view. The **NFC thread**
is owned by the firmware's poller: it calls the worker's callback from inside
its own loop.

- The worker callback runs on the NFC thread. It may not touch a view, so it
  copies its progress into the application object and posts a custom event to
  the view dispatcher. The scene picks the event up on the GUI thread and
  redraws.
- The poller callback returns `NfcCommandContinue` or `NfcCommandStop` and
  nothing else. Stopping the poller from inside its own callback deadlocks,
  so the scene does it after the event arrives.
- `emrtd_worker_stop()` is safe from the GUI thread and only sets a flag; the
  read ends at the next point the worker checks it.

The application's stack is 4 KB (`application.fam`), the NFC worker thread's
is 8 KB, and the elliptic curve arithmetic of PACE runs on the latter. That
is why nothing on the read path puts a large buffer on the stack.

## Memory

The heap is 186 KB, the application's own image occupies half of it before a
read begins, and a single data group can be forty kilobytes. So the read path
is written not to hold a file:

- `emrtd_tlv` parses in place. A node is a pointer into the buffer it came
  from, iteration is a cursor, and nothing is copied or allocated.
- The export is opened before the read starts. Each chunk that comes off the
  chip is written to the SD card and folded into a running hash as it passes,
  so DG2 exists in memory one frame at a time.
- The facial image is found from the first few hundred bytes of DG2 with
  `emrtd_lds_dg2_find_image()`, and from then on the same chunks are teed
  into a second file. The image is never a second copy of the group.
- A `.fap` is loaded into RAM, so the binary itself counts against that
  186 KB - 94,848 bytes of it, in blocks that have to be contiguous. See
  items 15 to 20 of [platform.md](platform.md).
- A read is refused before it starts if the heap cannot hold it, rather than
  allowed to reach an allocation that would take the device down: on this
  firmware a request the allocator cannot meet crashes rather than returning
  NULL. The commonest cause is a computer attached over USB, which costs about
  20 KB.
- Nothing collects garbage, in the firmware or here. Every allocation has one
  owner and one scope: the application, one read, one file, one step or one
  screen. It is freed where that scope ends. A block that is not freed stays
  allocated until the device restarts, because closing the application frees
  only the application's image and its thread. The allocator zeroes what it
  frees and what it hands out. Items 22 to 26 of [platform.md](platform.md)
  have the detail, and how to check a read for leaks on the device.

## Errors

One enumeration, `EmrtdError`, crosses every layer, and each value has a line
for the screen (`emrtd_error_text`) and a three line hint of what to try
(`emrtd_error_hint`), both measured against the screen's fonts by the host
tests. A status word that is not 9000 is mapped to the closest
value by `emrtd_error_from_sw()` and kept alongside its own text, so a
failure can name both the layer that noticed and the byte that caused it.
[troubleshooting.md](troubleshooting.md) is the same table, written out.

## Tests

`tests/host` builds `protocol/`, `crypto/`, `transport/` and `access/` with
the host compiler, the address and undefined behaviour sanitizers, and mbed
TLS 3.6.2 configured with the firmware's own `mbedtls_cfg.h`. Using the
firmware's configuration is what makes the results mean anything: it is why
the suite sees the same absent brainpool curves and the same missing CMAC the
device does. `make -C tests/host config-drift` compares the vendored copy of
that configuration with the deployed SDK, and the build workflow runs it with
`STRICT=1` so that a missing SDK fails rather than quietly comparing nothing.

The suite that reaches furthest is `test_session.c`: it drives a simulated
chip through the transceiver port, from choosing an access driver to parsing
EF.SOD, in full frames and in ninety-six byte ones.

### A second opinion from GCC

Continuous integration compiles the suite with GCC and a developer on macOS
compiles it with clang, and the two do not warn about the same things.
`-Wstringop-truncation` has no clang equivalent at all, so a `strncpy` that
drops its terminator passes locally and fails the build on push.

`make -C tests/host gcc-check` closes that gap. There is no host GCC on a Mac,
but the Flipper toolchain ships a real one for ARM, and a warning from the
middle end does not care what architecture it is generating code for. The
target compiles every translation unit with it and throws the objects away. It
cannot link or run anything, so it is no substitute for the real build - it
exists to catch the one class of failure that otherwise only appears in CI.

### What the suite does not cover

The block transmission layer is covered - `test_isodep.c` exercises it against
a card-side simulation - but the poller lifecycle around it is not: which
firmware events arrive in what order, and what `NfcCommandReset` does to a chip
that has entered the ISO 14443-4 protocol state, are reasoned about from the
firmware sources rather than executed here.

`test_session.c` drives its own read loop, not `worker/emrtd_worker.c`. The
worker is the one layer the host build cannot reach, because it is written
against the NFC stack and the FreeRTOS primitives, and it is also the layer
that decides what a status word means, when a session has died and when a file
is simply absent. Those are consequential decisions, and they are covered only
by reading the code.

That is not a theoretical concern. It is where the worst defect found in
review lived: a Secure Messaging session that died mid-read was reported to
the user as a completed one, because a bare status word from a chip that had
given up was read as an ordinary answer. The layers below behaved correctly
throughout - `emrtd_sm_unprotect()` refuses an unauthenticated response, and a
host test says so - and the whole suite still passed.

Anything moved out of the worker and into `protocol/` or `crypto/` becomes
testable by that move alone, which is the main reason to keep the worker thin.
