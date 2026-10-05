# When it does not work

## The application will not start

```
App Too Old: APP:87 < FW:88
```

The launcher is comparing the API version the package was built against with
the one the firmware provides, and refusing the mismatch. Nothing is wrong with
the application: the package was simply built for a different Flipper firmware
than the one on the device. `APP:87` is the official firmware and `APP:88` is
Unleashed or Momentum.

Take the package whose name matches your firmware from the release, or build
one:

```bash
ufbt update --index-url https://up.unleashedflip.com/directory.json   # Unleashed
ufbt update --index-url https://up.momentum-fw.dev/firmware/directory.json
ufbt update --channel=release                                          # official
ufbt launch
```

The sources are identical for all of them, and so is the mbed TLS
configuration these firmwares ship, so the reader behaves the same on each.

The mirror image, `App Too New`, means the opposite: a package built against a
newer API than the firmware. Update the firmware, or rebuild against the SDK
that matches it.

## The errors, one by one

| What the screen says | What happened | What to try |
| --- | --- | --- |
| **No document found** | Nothing answered in the field. | Lay the Flipper flat on the open data page, over the middle. If nothing answers, try the closed book with the back cover against the device: the chip is in one place or the other. Take the document out of any case and move a phone or a second card away. |
| **Document moved away** | The chip stopped answering part way through, and did not answer the retries either. | Hold both still until the progress bar fills. DG2 takes several seconds on its own. |
| **Chip would not connect** | The document answered the scan, and then would not complete RATS. | Lift the Flipper clear, lay it back on the data page and read again. If it fails every time, send a trace: the first line names the card's frame size and waiting time, which is what this failure is about. |
| **Radio exchange failed** | A frame did not come back. | The same as above. If it happens at the same point every time, the trace shows where. |
| **Garbled chip reply** | The answer did not fit ISO 14443-4. | Usually a marginal field rather than a faulty chip: move the document a little and read again. If it persists, attach a trace to a report. |
| **Not an eMRTD document** | The chip answered but carries no eMRTD application. | Bank cards, transport cards and access badges all answer, and none of them carry the application identifier `A0 00 00 02 47 10 01`. A passport carries it, and so does an EU identity card issued since 2021. |
| **Chip refused command** | A status word other than 9000 came back. | The screen does not show the status word; an APDU trace records it, and the table below says what it means. |
| **Not found on this chip** | `6A82` on a file. | Normal. Only DG1, DG2 and EF.SOD are mandatory; the rest are up to the issuing state, and EF.COM lists what is there. |
| **Not found on this chip** | `6A83` when the application is selected, or `6A82` when it is selected again inside the PACE session. On the error screen it means the chip would not open the eMRTD application. | Lay the document back on and read again. |
| **Chip refused access** | `6982` when the application is selected or the key is exchanged, which ends the read. The chip wants a session it does not have. | Lay the document back on and read again. If it keeps refusing, stop and check the key rather than retrying: see `6983`. |
| **Chip refused access** | `6982` on a file. The secure session is gone, or was never established. The file is marked and the read goes on. | Read again. If it happens on the same file every time, send a trace. DG3 and DG4 are never asked for: Extended Access Control needs a terminal certificate issued by a state, which no application on a Flipper can hold. |
| **Chip refused access** | `6983`. The chip has blocked itself after repeated wrong keys. | Only the issuer can clear that. |
| **Key not accepted** | The CAN, or the document number and the dates, are not the ones this chip was issued with. | See "the key looks right and it still fails" below. |
| **No way into the chip** | The access method is pinned to PACE, and the chip announces no PACE in `EF.CardAccess`. | Set **Access method** in Options to Automatic, so that BAC is tried with the number and the dates. |
| **PACE curve not usable** | The chip wants a curve above 256 bits, or one this reader does not know. | Nothing to do on the device: `MBEDTLS_ECP_MAX_BITS` is 256 in the firmware's mbed TLS. Pin the method to BAC and see whether the document also offers it. |
| **PACE mapping missing** | The chip wants the integrated or the chip authentication mapping. | Only the generic mapping is implemented, which covers nearly every document in issue. Try BAC. |
| **PACE DH not supported** | The chip runs PACE over a Diffie-Hellman group. | Impossible in an application on this firmware - see [platform.md](platform.md). Try BAC. |
| **PACE key rejected** | The chip's token did not match the one computed here. | Nearly always the password: a wrong CAN, or a wrong MRZ value. The CAN is the six digit number printed on the document, separate from the document number. |
| **Secure channel broke** | A response failed its checksum, or the sequence counter slipped. | The session cannot be resynchronised; read again. If it fails on the same file every time, send a trace. |
| **Malformed data** | A file, or a reply during PACE, is not the structure the standard describes. | A file is marked and its raw bytes are exported anyway: attach the `.bin` and the report. A reply during PACE ends the read: retry, and send an APDU trace if it repeats. |
| **Not supported** | Understood, but not implemented. | On a file, DG3 and DG4 are the usual cause: Extended Access Control needs a state issued terminal certificate. On the error screen it is a PACE variant this build does not implement; BAC, with the number and the dates, is the way in. |
| **Not enough memory** | The Flipper does not have the memory a read needs, so the read was refused rather than started. | Almost always a computer attached over USB. See the section below. |
| **SD card write failed** | The export could not be written. | Check the card is in, unlocked and has room. The read itself still works with **Export to SD** off. |
| **Value too large** | A field or a reply is bigger than the space reserved for it. | On a file, the raw file is exported in full, so nothing is lost; worth a report with that file. On the error screen it was a reply during PACE or on the radio: an APDU trace shows which. |
| **Details incomplete** | The credentials are missing or not well formed. | Up to twenty characters of A-Z and 0-9 for the number, and two dates that exist - or a CAN instead. **Document** on the error screen goes straight to them. |
| **Read cancelled** | You pressed back. | Nothing to do. Only `report.txt` says so, and the files read until then are kept in the folder. |
| **Internal error** | A library call failed in a way that should not happen. | Restart the application, and report it with the trace. |

Each of these is an `EmrtdError`. The error screen shows the heading and a
hint of three short lines under it, which is what fits above its buttons
without scrolling; this page is the background the screen has no room for.

## Status words

| SW | Meaning |
| --- | --- |
| `6282` | End of file reached before the requested length - usually harmless |
| `6300` | Authentication failed; the key is wrong |
| `6982` | Security status not satisfied; the file is there and the session lacks the rights for it - permanent on DG3 and DG4, otherwise a session that is gone or was never there |
| `6983` | Authentication method blocked; the chip has locked itself |
| `6A82` | File not found |
| `6A86` | Incorrect parameters P1-P2 |
| `6C xx` | Wrong length; the chip will accept `xx` bytes |
| `6D00`, `6E00` | Instruction or class not supported |

## "Document moved away" when nothing moved

This used to be the common case and it was the reader's fault, not the
document's. Up to version 1.0 the read ran over the firmware's ISO 14443-4A
poller, which gives a card 120 microseconds to answer every block when its ATS
carries no TB1, and 2.95 milliseconds to answer RATS. Neither is enough for a
eMRTD chip, neither is reachable from an application, and both failures arrive
as a timeout - which the reader reported as a document that had been taken
away. Since then the reader runs the block transmission protocol itself and
chooses its own waiting time, 295 milliseconds by default. Items 10 to 14 of
[platform.md](platform.md) have the detail.

So on a current build this message means what it says. If it still appears
with the document lying still, the trace is worth having: its first line
carries the ATS and the waiting time that was armed.

## The chip does not answer at all

1. A card goes flat against the back of the Flipper, face to face. A passport
   is opened at the data page, laid flat, with the Flipper on it face up over
   the middle of the page.
2. If nothing answers, close the book and put the back cover against the
   device. The chip is in one place or the other, and a few centimetres
   decide it.
3. Take the document out of any wallet or cover: a metal clip, a blocking
   lining or a second contactless card will all stop it.
4. Keep both still. The field is weak and the read takes seconds.

## The key looks right and it still fails

- **Check the document number character by character.** `0` and `O`, `1` and
  `I`. The number is the one in the MRZ at the bottom of the data page, not a
  number printed elsewhere on it. Type it without the check digit that follows
  it in the MRZ, letters included: the reader computes that digit itself.
- **Check the two dates.** The expiry is the document's, not a visa's, and a
  date of birth in the 1900s and one in the 2000s are different keys. Take
  them from the MRZ rather than from the printed lines: where the two
  disagree, the chip was keyed with the MRZ.
- **Try the CAN**, if the document prints one. It is a shorter string with no
  check digit, so there is less to get wrong.
- **Pin the access method** in Options. If PACE fails and BAC works, or the
  other way round, that is worth reporting.
- A document that has been refused too many times can block itself
  permanently (`6983`). If a key is not working, stop and check it rather
  than trying variations.

## The read stops part way

The error screen says where it stopped, under the hint: the file it died
on, or, when it stopped before any file, the stage - opening the document,
reading `EF.CardAccess`, or authenticating. `report.txt` carries the same
lines. The result screens still show everything that was read before. A failure on DG2
specifically is usually the document moving: it is the largest file and takes
the longest.

## Not enough memory, or the application dying when a read starts

If the reader worked yesterday and today it refuses to start a read - or, on a
version before this check existed, the Flipper reboots into an **Out of memory**
screen the moment the scan begins - look at the USB cable first.

**Close lab.flipper.net or qFlipper, unplug the cable, and restart the Flipper.**
Restarting is the part that matters: the application is loaded into RAM, so the
memory it wants has to be free before it is launched, and a session that has been
opened and closed does not always leave the heap as it found it.

### Why a cable costs so much

The Flipper's whole heap is 186 KB, and this application's own image occupies
94,848 bytes of it before a single passport is touched - a `.fap` is executed
from RAM, not from the SD card, so the binary is resident for as long as it
runs. A read then needs about 28 KB more, and one unbroken piece of 8 KB for the
radio thread.

A computer attached over USB takes about 20 KB, in three layers that arrive
separately:

| What | Cost | When |
| --- | --- | --- |
| A serial shell | ~5.6 KB | any program opening the port, including a plain terminal |
| An RPC session | ~11.8 KB | qFlipper, lab.flipper.net, the mobile application |
| The screen mirror | ~2.9 KB | lab.flipper.net starts it by itself on its front page |

Twenty kilobytes is almost exactly the margin a read has, which is why the same
document reads perfectly with the cable out and fails with it in.

### Narrowing it down without any equipment

lab.flipper.net's own pages cost different amounts, so moving between them
brackets the problem. Try the read from each, in this order:

1. the front page, which is the Device page - shell, session and screen mirror;
2. `/apps` or `/archive` - shell and session, no mirror;
3. `/cli` - the shell only, because opening a text terminal closes the session;
4. cable out, after a restart.

If it fails on the front page and works on `/apps`, the margin is under three
kilobytes and the screen mirror is what tips it over. If it fails on both and
works with the cable out, it is the shell and the session.

From `/cli`, the `free` command prints the answer directly. **Maximum heap
block** is the number that decides whether a read can start; it has to be above
8,200. It is a separate question from the free total, because the radio thread's
stack has to come out of one unbroken run, and a heap can have plenty free in
pieces too small to be of use.

### Reading the crash itself

On a version without the pre-flight check, the crash dump goes out the **log
UART on pins 13 (TX) and 14 (RX) at 230400 8N1** - which a computer holding the
USB port does not touch. That dump names the thread that failed, the size it
asked for in `r7`, and the free heap at that instant. Free heap far above the
requested size means the heap was fragmented rather than full; barely above it
means it was full.

## Sending a report

Turn on **APDU trace** in Options, reproduce the problem, and take `trace.txt` and
`report.txt` from the export directory:

```
/ext/apps_data/emrtd/<document>_<date>/trace.txt
```

The trace carries every command and every answer with the Secure Messaging
taken off them, and a note against each saying which file was being read, at
which offset, and what the chip answered - which is what makes a report about
one document actionable for somebody who does not have it.
[trace.md](trace.md) describes the file line by line, and has the path from a
trace to a change to the reader if you would rather make it yourself.

**Read the trace before you send it.** A trace of a failure before
authentication contains no document data; a trace of a read that got further
is a record of a session with your own document. There is a section about
exactly what is in one in [security.md](security.md).

Useful in a report, alongside the trace:

- the issuing country and the year the document was issued - not the number;
- what the security screen said about the access method and the curve;
- the application version, which the About screen shows, and the firmware
  version the Flipper is running.

Issues go to <https://github.com/filipsedivy/emrtd-flipperzero/issues>.
Security problems go through [SECURITY.md](../SECURITY.md) instead.
