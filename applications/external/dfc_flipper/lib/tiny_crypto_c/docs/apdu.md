<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Smart-card APDUs

Enable `TINY_CRYPTO_ENABLE_APDU=ON` and include `<tiny_crypto/apdu.h>`. The
module encodes ISO/IEC 7816-4:2020 command APDUs, reads response APDUs, classifies
status words and runs one command-response exchange over a caller transport. It
depends on no other module, allocates nothing and uses about 120 bytes of
stack on AVR. Card application commands, such as the PIV commands of SP 800-73-5 Part
2, build on this layer.

## Commands and responses

A `TC_APDU_command` holds CLA, INS, P1, P2, the command data as a borrowed span
(Nc bytes) and `ne`, the expected response length Ne. `ne` 0 omits the Le field.
`ne` 256 encodes the short Le `00` and 65536 the extended Le `0000`, or `000000` without
command data (section 5.2). `ne` is a `uint32_t`, so 65536 fits on targets with a 16-bit
`size_t`.

`TC_APDU_SHORT` uses one-byte Lc and Le fields and accepts Nc up to 255 and Ne
up to 256. `TC_APDU_EXTENDED` keeps the short form whenever both fields fit it,
and otherwise writes the 3-byte extended Lc with a 2-byte Le, or a 3-byte Le
without data. Select `TC_APDU_EXTENDED` only for a card that states extended
length support (sections 5.2 and 12.8.1). `TC_APDU_command_size` reports the
encoded size, and `TC_APDU_EXTENDED_COMMAND_BYTES(nc)` gives an upper bound.

The module accepts the first interindustry CLA values `00` to `1F` (section 5.4.1
Table 2), which cover secure messaging bits b4 b3 and logical channels 0 to 3.
Proprietary, further interindustry and RFU classes return `TC_APDU_UNSUPPORTED`.
The chaining bit b5 belongs to the channel, so a command with b5 set returns
`TC_APDU_ARGUMENT`.

`TC_APDU_response_read` borrows the response data and returns SW1 SW2 as one
`uint16_t`. It rejects a response shorter than 2 bytes, a status outside `6XXX`
and `9XXX`, any `60XX`, and data returned with SW1 `64` to `6F` (section 5.6).
`TC_APDU_status_classify` maps a status to the classes of section 5.6 Table 6.
The classes are informational. Card applications give specific values their
meaning.

## Transport

The application implements `TC_APDU_transmit`: send one command APDU and
receive one complete response APDU, data followed by SW1 SW2. The callback
writes at most `response.capacity` bytes, sets `*length` on `TC_OK` and returns
`TC_ERROR` when delivery failed or is uncertain. The library owns no I/O, reader
selection or timing. A PC/SC transport maps `SCardTransmit` onto this callback.

## Channel

`TC_APDU_channel_init` binds a transport, a scratch buffer and options. The
scratch buffer holds one encoded command fragment and is wiped after every
transmit, since commands such as VERIFY carry PIN digits.
`TC_APDU_SHORT_COMMAND_MAX_BYTES` (261) covers every SHORT command. The options
set the length format, the GET RESPONSE flags, the exchange budget and the
card's size limits from DO `7F66` (section 12.8.1). `TC_APDU_channel_restrict`
tightens those limits after the card reports them and replaces the flags.
`TC_APDU_channel_clear` wipes the scratch and the channel.

`TC_APDU_transceive` sends one logical command:

- SHORT command data above 255 bytes goes out as 255-byte fragments with CLA
  b5 set and no Le, then the last fragment with the command CLA and Le (section
  5.3.3). Each intermediate answer must be `9000` without data. Another status
  without data, such as `6883`, `6884` or `6982`, ends the exchange with
  `TC_APDU_OK` and that status. Data or a `62XX` or `63XX` warning on an
  intermediate answer is `TC_APDU_INVALID`. The channel checks the whole chain
  against the scratch buffer, the card limit and the exchange budget before the
  first fragment, so these limits never interrupt a chain. EXTENDED commands
  are sent whole.
- A final `61XX` is followed by GET RESPONSE with Le = SW2, and each chunk is
  appended to the response buffer (section 5.3.4). SW2 `00` requests 256 bytes.
- A `6CXX` answer without data to a command with Le re-issues the same step
  once with Le = SW2 (section 5.6). Each GET RESPONSE step gets its own
  correction. A second `6CXX` on the same step, and `6CXX` on a chained command,
  on a command without Le or under secure messaging, end the exchange with that
  status and the data collected before it. A command without Le, such as a
  VERIFY carrying PIN digits, reaches the card once.
- The card may return more than Ne bytes. Each transmit offers the remaining
  response capacity, and TWIC Part 2 v5 section 5.2 note 2 describes cards that
  return a whole object. Object framing and secure messaging MACs carry the
  integrity checks.
- Each transmit consumes one exchange of the channel budget.

On `TC_APDU_OK`, `out->data` borrows the response buffer and `out->sw` holds the
final status of any class. Treat a non-success status as the card's answer to
the command. `TC_APDU_LIMIT` reports a response buffer, scratch buffer, card
limit or exchange budget that ran out. The response buffer is wiped and the
channel stays usable, and the next command ends the interrupted chain.
`TC_APDU_INVALID` reports a malformed card answer and wipes the buffer.
`TC_APDU_ERROR` reports a transport failure or a transport that broke its
length contract. The channel then stops and returns `TC_APDU_ERROR` until the
next `TC_APDU_channel_init`.

### GET RESPONSE flags

ISO/IEC 7816-4 section 5.6 permits GET RESPONSE with the command CLA.
SP 800-73-5 Part 2 sections 4.2.6 and A.4.1 send it with CLA `00` after secure
messaging and chained commands. `TC_APDU_GET_RESPONSE_PLAIN_CLA` clears the
secure messaging and chaining bits and keeps the logical channel. The 6CXX rule
still follows the command CLA, so a plain GET RESPONSE after a protected command
keeps the secure messaging behaviour.

After `61 00` the channel requests 256 bytes with Le `00` (ISO/IEC 7816-4
section 5.3.4, TWIC Part 2 v5 Appendix E). A TWIC NEXGEN card answers Le `FF`
after `61 00` with 255 bytes and `9000` and drops the rest of the object, so
TWIC Part 2 v5 section 5.2 note 3a does not describe its GET RESPONSE.

## Example

```c
#include <tiny_crypto/apdu.h>

/* Read the PIV Discovery Object (SP 800-73-5 Part 2 section 3.1.2) through a
 * caller transport. On TC_APDU_OK, object->sw holds the card status (9000 on
 * success) and object->data borrows response. */
TC_APDU_result read_discovery(TC_APDU_transport transport, uint8_t* response,
                              size_t capacity, TC_APDU_response* object)
{
  static const uint8_t tag_list[] = {0x5c, 0x01, 0x7e};
  uint8_t scratch[TC_APDU_SHORT_COMMAND_MAX_BYTES];
  const TC_APDU_channel_options options = {TC_APDU_SHORT, TC_APDU_GET_RESPONSE_PLAIN_CLA,
                                           8, 0, 0};
  const TC_APDU_command get_data = {{tag_list, sizeof tag_list}, 256, 0x00, 0xcb, 0x3f, 0xff};
  TC_APDU_channel channel;
  TC_APDU_result result = TC_APDU_channel_init(&channel, transport, &options,
                                               (TC_buffer){scratch, sizeof scratch});
  if (result != TC_APDU_OK)
    return result;
  result = TC_APDU_transceive(&channel, &get_data, (TC_buffer){response, capacity}, object);
  /* Wipe the scratch buffer on every path. LIMIT, INVALID and ERROR have
   * already wiped the response buffer. */
  TC_APDU_channel_clear(&channel);
  return result;
}
```

The C++11 functions `tiny_crypto::apdu_command_size`, `apdu_command_encode`,
`apdu_response_read` and `apdu_status_classify` in `<tiny_crypto/apdu.hpp>` wrap
the codec.

## Limits

- Nc is at most 65535 and Ne at most 65536. The response data is at most the
  response capacity minus 2.
- The exchange budget bounds the number of C-RPs for the channel lifetime.
- T=0 TPDU handling, extended-length chaining, proprietary classes, logical
  channels above 3 and secure messaging belong to other layers.

## Resource use

The channel keeps no static state. The caller owns the channel, the scratch
buffer and the response buffer. On an ATmega328P with avr-gcc 7.3.0 at `-Os`,
`TC_APDU_transceive` needs about 120 bytes of project stack, excluding the
transport callback. The `apdu_piv_read` profile of `tests/budgets/avr.json`
records the codec with the PIV card commands on an ATmega2560, and
`test_apdu_piv_read_qemu_avr` runs the 16-bit length cases on an emulated
Arduino Uno ([AVR builds and budgets](testing.md#avr-builds-and-budgets)).
