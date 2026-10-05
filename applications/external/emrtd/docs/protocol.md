# The protocol on the wire

An eMRTD is a smart card that happens to be contactless. Everything below is
ISO/IEC 7816-4 command and response pairs carried over ISO/IEC 14443-4, with
the commands and the order taken from ICAO Doc 9303 parts 10 and 11.

## What the firmware does, and what is left to us

The firmware's NFC stack finds the card and runs anticollision and selection.
Above that the two flavours are handled differently, and the reason is
measured rather than stylistic.

On **type A** this reader runs the block transmission protocol itself, in
`transport/emrtd_isodep.c`: RATS and the ATS, block numbering, chaining in
both directions, the waiting time extension a chip asks for while it does
elliptic curve arithmetic, and recovery of an answer that was lost on the way.
That recovery is R(NAK), or the same R(ACK) again while the card is chaining
its answer, as ISO/IEC 14443-4 prescribes. The lost block is sent again only
when the card has answered that it never had it: rule D has the card execute
every I-block it receives, whatever its block number, so a second copy sent
unasked is a second command, and under Secure Messaging that ends the session.
The firmware has all of that too, and it cannot be used, because it
gives every block a frame waiting time of 120 microseconds whenever the card's
ATS carries no TB1, allows the answer to RATS only 2.95 milliseconds, and
exposes no way for an application to change either. Items 10 to 12 of
[platform.md](platform.md) give the lines that establish it. A passport loses
those races while lying perfectly still on the device, and the reader used to
report that as a document that had been moved away.

On **type B** the firmware's ISO 14443-4B poller is used unchanged. Its
waiting time comes from the protocol info of the ATQB and is correct, and that
path does not reassemble a chained response.

The Flipper announces **FSD = 256** in RATS, which is the largest frame it
will accept. A response longer than that is chained by the card. The type A
path now follows such a chain, but the type B path cannot, so the reader
continues to size every question so that the answer fits one frame - it costs
nothing, and it keeps one rule for both flavours:

```c
size_t emrtd_transceiver_max_le(const EmrtdTransceiver* transceiver, size_t block_size);
size_t emrtd_transceiver_max_lc(const EmrtdTransceiver* transceiver);
```

`max_le` starts from `fsd`, subtracts what Secure Messaging will add around
the answer - DO'87' with its padding indicator and length, DO'99' and DO'8E' -
and rounds down to the cipher block, so that the padded plaintext fits
exactly with nothing left over. `max_lc` does the same in the other
direction, bounded by `fsc`, the frame size the card itself announced in its
ATS. A card that announces nothing is treated as FSC = 32, which is what ISO
14443-4 says to assume.

Short length encoding is used throughout. Extended length would let a command
ask for more than 256 bytes, but the frame could not carry the answer, so
there is nothing to gain.

## The commands

| Command | CLA INS P1 P2 | Where |
| --- | --- | --- |
| SELECT application by AID | `00 A4 04 0C` | 9303-11, 4.2 |
| SELECT elementary file by identifier | `00 A4 02 0C` | 9303-10 |
| READ BINARY, offset in P1-P2 | `00 B0 <offset>` | ISO 7816-4 |
| READ BINARY by short identifier | `00 B0 80\|sfi <offset>` | ISO 7816-4, 5.4.2 |
| GET CHALLENGE | `00 84 00 00`, Le 8 | BAC, 9303-11, 4.3 |
| EXTERNAL AUTHENTICATE | `00 82 00 00`, Lc 40, Le 40 | BAC |
| MSE:Set AT | `00 22 C1 A4` | PACE, 9303-11, 4.4.4 |
| GENERAL AUTHENTICATE | `00 86 00 00`, `10 86 00 00` when chaining | PACE |

The application identifier is `A0 00 00 02 47 10 01`. `EF.CardAccess` is file
`011C` in the master file and is readable before any authentication - it has
to be, because it is how the chip says which protocol it wants.

## The sequence of a read

```
   SELECT application (AID)
   SELECT EF.CardAccess, READ BINARY          -> SecurityInfos, if present
   |
   +- PACE                                     or  +- BAC
   |    MSE:Set AT   OID, password reference    |     GET CHALLENGE      -> RND.IC
   |    GA  encrypted nonce      7C 80 / 7C 80  |     EXTERNAL AUTHENTICATE
   |    GA  map nonce            7C 81 / 7C 82  |        E.IFD || M.IFD  -> E.IC || M.IC
   |    GA  key agreement        7C 83 / 7C 84  |
   |    GA  mutual authentication 7C 85 / 7C 86 |
   |    SELECT application again, now protected |
   |
   Secure Messaging from here on
   SELECT EF.COM,  READ BINARY ...             -> the table of contents
   SELECT EF.SOD,  READ BINARY ...             -> the hashes
   SELECT EF.DGn,  READ BINARY ...             -> each announced group
```

The steps of GENERAL AUTHENTICATE are nested in a dynamic authentication
template, tag `7C`; the request and the response use different inner tags,
`80`/`81`/`83`/`85` from the terminal and `80`/`82`/`84`/`86` from the chip.
Every step but the last sets bit 4 of CLA, which is command chaining: the
chip knows more is coming.

PACE leaves the master file selected, so the application is selected again
afterwards - inside Secure Messaging this time. BAC does not, which is what
`reselect_application` on the access driver expresses.

## Reading a file

A file is read in three moves:

1. `SELECT` it by its two byte identifier.
2. `READ BINARY` the first four bytes. Those are the BER-TLV header, and
   `emrtd_tlv_total_length()` turns them into the size of the whole file
   without a length being asked for anywhere.
3. `READ BINARY` the rest in chunks of `max_le`, at increasing offsets, until
   the total is reached.

Two answers are not failures and have to be handled:

- **`6C xx`** - wrong length. The chip is telling you the length it will
  accept; repeat the command with `Le = xx`.
- **a short read** - a chip may return fewer bytes than asked for. The offset
  advances by what actually arrived, not by what was requested.

Files may also be addressed by short identifier, which saves the SELECT. The
short form carries the offset in P2 alone, so it only reaches the first 256
bytes; it is useful for `EF.CardAccess` and little else.

### The limit worth knowing

In the short form of READ BINARY the offset lives in P1 and P2 with the top
bit of P1 reserved to mean "P1 holds a short file identifier". The reachable
offset is therefore **0 to 32767**. A data group larger than that - and a DG2
with a good quality portrait can be - cannot be finished with this command at
all. The way out in ISO 7816-4 is the odd instruction `B1`, which carries the
offset as a data object in the command field, at the cost of an envelope in
every response.

## Status words

| SW | Meaning | Reaction |
| --- | --- | --- |
| `9000` | Success | - |
| `6282` | End of file before Le bytes | Keep what came back; the file is finished |
| `63 00` | Authentication failed | The key does not open this chip |
| `6982` | Security status not satisfied | Secure Messaging is required, or has been lost |
| `6983` | Authentication method blocked | The chip has locked itself; only the issuer can help |
| `6A82` | File not found | The document does not carry that data group |
| `6A86` | Incorrect P1-P2 | The offset or the parameters are out of range |
| `6C xx` | Wrong Le | Repeat with `Le = xx` |
| `6E00`, `6D00` | Class or instruction not supported | Not an eMRTD application, or the wrong state |

`emrtd_error_from_sw()` maps these onto the application's own error values and
`emrtd_sw_text()` keeps the words, so a failure can be reported as both.

## Secure Messaging on the wire

Once a session exists, every command is rebuilt (ICAO 9303-11, 9.8):

```
  CLA |= 0x0C                       the header says "protected"
  DO'87'  01 || E(KS_Enc, padded command data)      present if there is data
  DO'97'  the original Le                            present if there was one
  DO'8E'  the first eight bytes of MAC(KS_MAC, SSC || padded header || DOs)
```

and every response is taken apart the same way: `DO'87'` with the encrypted
answer, `DO'99'` with the real status word, `DO'8E'` with the checksum over
the two. The checksum is verified **before** anything is decrypted; a
response that fails it is not looked at.

The send sequence counter is incremented before the command and again before
the response, so the two sides stay in step. There is no way to resynchronise
a counter that has slipped: the session is dead and the read starts again.

The details of the padding, the counter and the key derivation are in
[cryptography.md](cryptography.md).
