# The APDU trace

A document that reads on one device and not on another differs somewhere in
the exchange, and never in the data groups themselves. It is in which command
the chip refuses, how much of a file it will part with at a time, which of the
two `READ BINARY` forms it implements, or where it decides a file has ended.
`trace.txt` is that exchange written down, and this page is how to read one,
how to send one, and how to turn one into a change to this reader.

## Turning it on

**Options -> APDU trace -> On.** The trace is written into the export
directory, so **Export to SD** has to be on as well; with exporting off the row
reads `Needs export` and nothing is written. The file lands next to everything
else the read produced:

```
/ext/apps_data/emrtd/<document>_<date>/trace.txt
```

It is off by default. It is a diagnostic tool, it records the chip's answers in
the clear, and [security.md](security.md) says what that means before you send
one to anybody.

It also slows the read down. Every exchange under a session is written twice,
once as it crossed the radio and once with the envelope off, and each of those
goes to the SD card from the thread that is talking to the chip. On DG2 - two
hundred odd exchanges - that is seconds rather than milliseconds, so turn it
off again once you have the file.

## The shape of a line

A trace is four kinds of line and nothing else, which is what lets a tool read
one as well as a person:

| Line | What it is |
| --- | --- |
| `> <hex>` | A command as it went out on the wire |
| `< <hex>` | A response as it came back |
| `>> <hex>` | A command before Secure Messaging wrapped it |
| `<< <hex>` | A response after Secure Messaging was taken off it |
| `# <event> [key=value ...] [text]` | A note |

The `>` and `<` lines are the ground truth: the bytes the radio moved, whatever
they meant. The `>>` and `<<` lines appear only while a session exists, because
without one the wire already carries the command in the clear and a second copy
of it would say nothing.

Both kinds of response line end in the status word. On the wire that is where
the chip put it; inside Secure Messaging it arrives in `DO'99'` instead, and the
reader writes it back onto the end so that one rule - *a response line ends in
its status word* - holds for every line in the file.

A note is a `#`, an event name, then fields of the form `key=value` whose values
never contain a space, then, on the events that have one, a human sentence. A
parser splits on whitespace, switches on the first token, and stops reading
fields at the first token without an `=` in it.

## The notes

| Event | Fields | Written when |
| --- | --- | --- |
| `trace` | `format`, `reader` | The first line. The format number, and the version that produced the file |
| `legend` | - | The three lines under it, so that a trace explains itself |
| `notice` | - (a sentence) | What the file contains, said in the file itself |
| `card` | - (a sentence) | A card was activated. Frame size, waiting time, ATS - what a failure on a document that never moved is usually about |
| `select` | `target=application` | The eMRTD application is being selected |
| `select` | `target=file`, `name`, `fid` | A file is being selected. Everything below the line belongs to this file |
| `session` | `protocol`, `result=open` | An access protocol succeeded, with what it negotiated |
| `session` | `protocol`, `result=failed` | It did not, and why |
| `file` | `name`, `size`, `chunk` | The first answer of a file arrived: how long the file says it is, and how much of it this reader asks for at a time |
| `read` | `off`, `le`, `form` | A `READ BINARY`. `form=short` carries the offset in P1-P2, `form=odd` in `DO'54'` with instruction `B1` |
| `retry` | `off`, `le`, `asked` | The chip answered `6CXX` and named the length it would give |
| `recover` | `lost`, `radio` (a sentence) | The `<` line above arrived, but not first time: `lost` frames went unanswered or arrived damaged and the answer was asked for again. `radio` lists the radio's code for each, comma separated |
| `sw` | `code` (a sentence) | Every response that could be read, with what the status word means. A response whose envelope failed to verify has no status word to report and gets an `error` note instead |
| `eof` | `reason`, `at` | A file ended: `6282`, `6B00`, or an answer with no data in it |
| `skip` | `name`, `reason` | A group that was never asked for: `eac`, or `deselected` in Options |
| `done` | `name`, `state`, `bytes`, `hash` | One line per file, whatever became of it |
| `error` | `stage`, `sw` (a sentence) | Something went wrong, at `exchange`, `secure-messaging`, `read`, `size`, `decode` or `card-access`. `sw` appears on the one that carries an unprotected status word |
| `end` | `files`, `hashes` (a sentence) | The last line. A trace that stops before it stops because the card did |

`state` is one of `read`, `absent`, `failed`, `skipped`; `hash` is `match`,
`mismatch`, `not-listed`, `unsupported-digest` or `unchecked`. Both are one
token and will stay one token.

## A read, annotated

```
# trace format=2 reader=1.0.0
# legend > command, < response, on the wire
# legend >> command, << response, inside Secure Messaging
# legend # a note: an event and its fields
# notice this file carries the document's own data in the clear; read docs/security.md before sending it anywhere
# card Type A, FSC 256, FWI 8 waiting 77 ms, SFGI 0 guard 0 ms, ATS 0578807002
# select target=application
> 00A4040C07A0000002471001
< 9000
# sw code=9000 OK
```

Nothing is protected yet, so there are no `>>` lines: the wire is the command.
`EF.CardAccess`, and then PACE or BAC, follow in the same clear form. Then:

```
# session protocol=PACE result=open PACE ECDH-GM/AES-128, brainpoolP256r1
# select target=application
>> 00A4040C07A0000002471001
> 0CA4040C1D87110152F3...8E08C5A9...
< 990290008E0844B7...
<< 9000
# sw code=9000 OK
# select target=file name=EF.DG1 fid=0101
>> 00A4020C020101
> 0CA4020C1D8711017BD4...8E08...
< 990290008E08...
<< 9000
# sw code=9000 OK
# read off=0 le=208 form=short
>> 00B00000D0
> 0CB000000D9701D08E08...
< 87610126A8...990290008E08...
<< 615B5F1F58...9000
# sw code=9000 OK
# file name=EF.DG1 size=93 chunk=208
# done name=EF.DG1 state=read bytes=93 hash=match
```

PACE leaves the master file selected, so the application is chosen again inside
the session; that is the first protected exchange above, and it is the same
command as the unprotected one at the top of the page.

The `>>` line is the command this reader meant; the `>` line under it is what
the chip was actually given. That pairing is the whole point of the file: the
top line says `READ BINARY at offset 0 for 208 bytes`, and the bottom one is
the only thing a second implementation can be compared against.

`chunk=208` is not a constant. It falls out of the frame sizes and the cipher
block - 208 under AES with a 256 byte frame, 224 under 3DES, 250 with no
session at all - and a card that announces a small frame gets small reads. It
is the first number to look at when a file arrives short or takes an unexpected
number of rounds.

## Reading one when a document misbehaves

- **A file that stops early.** Find its `# file` line for the length the file
  claims, then its `# done` line for the length that arrived. In between, the
  last `# read` before the stop says where, and the `# sw` under it says what
  the chip answered. `eof reason=6282` is the chip saying it ran out before
  filling `Le`, which is ordinary; `eof reason=empty` at an offset short of the
  size is not.
- **A file that takes far more rounds than its size suggests.** Look for
  `# retry`: a chip that answers `6CXX` to every read is dictating a length,
  and `asked=` is the length it wants.
- **Nothing past 32767 bytes.** `form=odd` appears once the offset outgrows
  P1-P2. A chip that refuses instruction `B1` leaves an
  `# error stage=read` line and a large DG2 that ends exactly there.
- **A session that dies mid read.** `# error stage=secure-messaging` names it.
  A two byte unprotected answer is the chip saying the session has gone
  (`6987`, `6988`); a checksum that did not verify is a different failure, and
  the `<` line above the note is the evidence for either.
- **A document that reads only just.** `# recover` lines mean answers are
  being lost and recovered. A few on DG2 suggest a marginal field; one under
  every PACE step points rather at a chip that draws more than the field gives
  while it computes.
  Radio code `7` is a timeout, `1` an answer that arrived damaged - the
  firmware names that one "not present" - and `6` a bad CRC. This reader
  reports `7` and `1` alike as "Document moved away", which is why the
  codes are worth having.
- **A read that never gets that far.** `# card` carries the frame size and the
  waiting time the reader armed, and `# error stage=exchange` carries what the
  radio said. A document that answers the scan and then stops is nearly always
  one of those two numbers.

## Turning a trace into a change

This is the path from a document that will not read to a reader that reads it,
and it does not need the document again after the first step.

1. **Find the exchange that behaved differently.** One `# select`, its `# read`
   lines, and the `# sw` under them. The trace is what replaces having the
   document: from here on, nobody needs it in their hand.
2. **Teach the simulated chip to behave that way.** `sim/emrtd_sim.c` is a
   passport at the APDU level, written from the standard rather than from this
   reader, and it is where a quirk goes: the status words it answers are named
   at the top of the file, `EmrtdSimConfig` already carries `fsc` and `fsd`, and
   a chip that insists on `6CXX`, refuses instruction `B1` or ends a file with
   `6282` is a few lines inside its `READ BINARY` handler. This is the step
   that turns one person's document into something the repository keeps.
3. **Change the layer the behaviour belongs to**, and know which of them a
   workstation can check. The host suite compiles `protocol/`, `crypto/`,
   `transport/` and `access/`, so a fix to what a command looks like, how a
   file parses or how the frame arithmetic works gets a test in
   `tests/host/` that fails before it and passes after -
   `tests/host/test_session.c` is the one that drives a read end to end
   against the simulated chip, and `make -C tests/host` runs the lot.
4. **A fix in `worker/emrtd_worker.c` is different, and this is the honest
   part.** The read loop - the chunk size, the `6CXX` retry, the fall back to
   instruction `B1`, where a file is decided to have ended - is bound to
   `furi`, `storage` and the NFC stack, so it is not in the host build and
   there is no test on a workstation that covers it. It is verified on the
   device, against the document, with the trace switched on: the same read that
   produced the report should now show the exchange going the other way. Say so
   in the pull request, and attach the two traces.
5. **Keep the simulated quirk either way.** A chip behaviour that is written
   down in `sim/` is one that no later change can quietly forget about, even
   when the fix above it could not be tested on a host.

If you are filing this rather than fixing it,
[troubleshooting.md](troubleshooting.md) says what to send with it.

## What is never in a trace

No key, no nonce, no session key, and none of the three credentials. The notes
carry file names, offsets, lengths and status words; the hex carries what the
chip sent and what it was sent.

That is not the same as harmless. Everything a session carries is in the `<<`
lines in the clear - which is the point, and is also the whole of the data page.
Read a trace before you send it, and read
[security.md](security.md) first.

## The format number

`# trace format=2` is the first line so that anything reading a trace can check
it before anything else. It moves when a line changes shape - a new note does
not change the shape of anything, a fifth kind of line would. The number lives
in `worker/emrtd_export.c` beside the code that writes the header, and this
page is what it refers to.

Format 1 was the same `>` and `<` lines with free text notes under them and no
version line; a trace with no `# trace` first line is one of those, and its
`SW` notes on protected exchanges were read off the end of the checksum and
mean nothing.
