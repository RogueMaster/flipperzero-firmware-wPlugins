# Handling the data

This application reads real travel documents. What needs protecting is not a
token that can be rotated - it is an identity, and the key to it is printed on
a page that cannot be reissued cheaply. This page says what is sensitive, what
the repository does about it, and what is left to you.

## What counts as sensitive

| Material | Where it is | Why |
| --- | --- | --- |
| Document number, date of birth, date of expiry | typed on the device, kept in `emrtd.settings` unless you say otherwise | These three **are** the key. `Kseed = SHA-1(MRZ information)`, and PACE derives its password from the same string: anyone holding them can open that chip. |
| Card access number | the same | The PACE password in its own right. |
| `KSenc`, `KSmac`, the initial SSC | in memory for as long as the app is open, shown by **Result -> Keys** | The Secure Messaging session itself. With a trace of the same read they would decrypt it; that is why they are never written anywhere. |
| `EF_DG*.bin`, `EF_SOD.bin` | the export directory | The exact bytes of the chip, the facial image among them. |
| `face.jpg` / `face.jp2` | the export directory | Biometric data in the ordinary legal sense of the term. |
| `mrz.txt`, `report.txt` | the export directory | The same identity in a form anything can read. |
| `trace.txt` | the export directory | The exchange itself. See below. |

## Where it lives on the device

Everything is under `/ext/apps_data/emrtd/` on the SD card - the exports in a
directory per read, and the credentials in `emrtd.settings` unless **Remember
on SD** has been turned off.

That settings file is plain. It is not encrypted, and it cannot usefully be:
the device has no secure element, no user secret to derive a key from, and
anything the application could unlock unattended, a reader of the card can
unlock too. A Flipper is a small object that gets left on desks and lost in
bags, and the SD card can be taken out and read on any computer.

So the reader is plain about it rather than clever. **Remember on SD** is on by
default, because the card is the only place the values survive the app being
closed and a reader that asks for a document number, a date of birth and a date
of expiry before every read is a reader nobody uses. What that costs you is
stated here, and there are two ways out, both deliberate and both one screen
away:

- **Options -> Remember on SD**, turned off, stops anything of the key being
  written from then on - you type the values for each read instead;
- **Document -> Forget stored data** removes what is already there. That
  deletes the settings file and clears the copy in memory.

The credentials also stay in memory for as long as the app is open, whichever
way that switch is set. A read that succeeded does not clear them - the next
document of the same person needs them, and a read that *failed* needs them for
what the error screen shows and for **Retry**. Closing the app wipes them.

A settings file written by a build with a different file format is discarded
*and removed* rather than left in place, because it may hold credentials under
keys this build no longer reads - and Forget would not know to clear them.

An export is deleted with the Flipper's own file manager, or from a computer
with the SD card in it; the reader browses its exports but does not erase
them. A read with **Export to SD** off never writes one in the first place -
that switch is the master one, and the APDU trace cannot write past it.

## The trace

`trace.txt` is written only when you ask for it, and it is the right thing to
send with a bug report - but read it first. What is in it depends on how far
the read got:

- everything before a session exists is in the clear: the SELECT commands,
  `EF.CardAccess`, and the BAC or PACE exchange itself;
- that exchange does not contain the credentials, but it is derived from
  them, and the encrypted nonce and the authentication tokens are material an
  attacker with the document in hand could test a guess against;
- **once Secure Messaging is running, the trace carries both forms**: the
  ciphertext as it crossed the radio, on the `>` and `<` lines, and the same
  exchange with the envelope taken off it, on the `>>` and `<<` lines. The
  second is the readable one, and it is the whole reason the file is worth
  sending: a trace that is ciphertext from the session onwards says nothing
  about the read that went wrong. It also means the data groups are in there in
  the clear - the machine readable zone, the additional details, and every
  byte of the facial image as it streamed past.

No key, no nonce and none of the three credentials is ever written to a trace;
what is written is what the chip sent. That makes a trace no more sensitive
than the `EF_DG*.bin` files beside it, and no less: the two come out of the
same read and **Export to SD** is the one switch over both.

A trace of a failure before authentication is nearly always safe to share. A
trace of a successful read is a record of a session with your own document;
treat it as you would the export. [trace.md](trace.md) describes what each line
of one means.

## What the repository does

`.gitignore` keeps the obvious things out of git: the build output, the
`ufbt` state, `/dumps/`, and `*.bin` anywhere in the tree - which covers every
raw file an export produces.

It does **not** cover the decoded output. `report.txt`, `mrz.txt`,
`trace.txt` and `face.jpg` are ordinary names that a commit would carry
without complaint. The repository cannot guess where you might copy an export
to, so the rule is simpler than a pattern: **do not put an export inside a
clone of this repository.** Keep it somewhere else, and if you must attach
something to an issue, attach the one file you mean to send.

The test material in this repository is the specimen identity ICAO publishes -
ANNA MARIA ERIKSSON of Utopia, document `L898902C` - which is exactly why it
can be committed.

## In memory

The working session keys live in an `EmrtdSm`, a member of the worker, and are
wiped by `emrtd_sm_clear()` the moment the session ends - on failure, on the
next driver attempt, and when the read finishes.

A **copy** of `KSenc`, `KSmac` and the counter the session started from is
taken the instant the session opens, before the first protected command moves
the counter on, and kept in the read result so that **Result -> Keys** can show
them. That copy is the one exception to the sentence above: it lives as long as
the application does. It is wiped when the app closes, and again at the start of
the next read. It is never written to the card.

The credentials live just as long, whether or not they are being remembered on
the card. A read that succeeded does not clear them and a read that failed must
not: the error screen shows what was used and Retry runs again with it.

Wiping is `emrtd_secure_wipe()`, which is `mbedtls_platform_zeroize()` and not
`memset()`. The difference is not pedantry: a `memset()` over a buffer that is
never read again is a dead store, and at the `-Os` every FAP is built with, the
compiler deletes it.

The allocator does part of this job on its own, and saying otherwise would be
as untrue as claiming a wipe that is not there. The official firmware,
Unleashed and Momentum all zero every heap block when it is freed and again
when it is handed out (item 23 of [platform.md](platform.md)). So nothing this
application frees carries its contents into the next application. What the
allocator cannot reach is memory still in use: a buffer that is still
allocated, and the stack of a thread that is still running, which keeps the
frames it has returned from until something overwrites them. The key derivation
and the secure messaging run on the NFC stack's thread for the whole read. The
wipes of memory that stays allocated are the ones that do work of their own:
those stack buffers, the session in the worker, the read result, which is
reused rather than freed, and the screens' shared text store. A wipe just
before a `free()` repeats what the allocator is about to do. It is kept for a
build of the firmware without the clear. Even there it would matter only to
something that reads freed memory directly, such as a debugger or a
use-after-free, because `malloc` clears what it hands out whatever the flag
says.

The honest limit is what a screen holds while it is open. **Result -> Keys**
builds its text in a string and hands it to a widget, which makes copies of its
own. The same is true of every screen that shows the holder's name or the
machine readable zone. A saved report is shown by a text box, which reads the
string it is given and copies only the page on screen. All of these copies sit
in the heap in the clear until the screen is left, and the result they were
drawn from stays there as long as the application runs. Leaving the screen
frees them, and on the three firmwares above, freeing them clears them. On a
build without the clear, the string Keys overwrites itself would be gone, but
the widget's copies, and the smaller buffers the string outgrew while it was
being built, would stay in freed heap until they were reused. A restart does
not clear the heap either. What a screen or the read held when the device
crashed or rebooted stays in RAM until something overwrites it (item 23 of
[platform.md](platform.md)).

The keys never reach the export or the trace, and that is a rule the code is
built to rather than a description of it: `EmrtdReadResult` is the struct
`emrtd_export_write_report()` reads, so the field holding them carries a
warning saying what must not be added next to it. A trace on the card beside
the keys that decrypt it would turn an export into the session in the clear.
The **password** is a different
matter, and the sentence that used to stand here was wrong about it: the MRZ
password *is* the document number and the two dates, and all three are written
into `mrz.txt` and `report.txt` in the clear, because they are part of the
document's own data. The export directory is named after the document number
as well. Treat an export as the identity it contains.

## What a read does and does not prove

The data group hashes are checked against EF.SOD, so a read can tell you that
the files on the chip belong together and have not been altered since it was
issued. The signature over EF.SOD is not verified here - that needs RSA and a
list of trusted country signing certificates, and neither is available on the
device. **A read cannot tell a genuine document from a competent copy of
one.** If that distinction matters for what you are doing, do the passive
authentication off the device, from the exported `EF_SOD.bin` and the raw
groups next to it.

## Before you point this at a document

Access requires physical possession, because the key is printed inside. That
is the whole protection model of BAC and PACE, and it is why this is not a
tool that can be used across a room. It also means possession is not consent:
use it on your own document, or with the explicit and informed agreement of
the person whose document it is. A facial image is biometric data, and the
rules where you are apply to whatever you export, however it was obtained.

## Reporting a problem

Security issues go through the process in [SECURITY.md](../SECURITY.md), not
through a public issue.
