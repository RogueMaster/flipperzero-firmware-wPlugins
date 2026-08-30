## Unreleased
 - Verify the message authentication code on received secure messages. It was
   never checked in either direction, so any message could be altered in flight
   and a session holding the wrong keys returned whatever the ciphertext
   happened to decrypt to
 - Validate every length taken off the wire before using it. A short frame
   claiming a long cryptogram read past the buffer, an empty one underflowed the
   padding loop, and a misaligned one left the plaintext buffer uninitialised
 - Use random session nonces. Every role used a constant, so all sessions
   derived the same keys and started the sequence counter at the same value, and
   any recorded exchange replayed
 - Answer every data command. Anything other than the one tag list naming the
   SIO file got no reply at all, not even a status word
 - Add response chaining in both directions, so a credential larger than one
   frame can be read and written
 - Accept a write on the card side. The reader has always sent one
 - Load the two BLE stacks as plugins, so they cost nothing until a screen asks
   for one. The resident image drops by about a third
 - Put the external dongle behind a saved setting. The flag selecting it was
   hardcoded off and never written, so that whole stack was unreachable
 - Add a host test suite, run on every push

## 1.3
 - Add Seos write support
 - Add Key Switcher feature
 - Fix Bluetooth Emulation
 - Add keys v2 support with per-device encryption
 - Fix compatibility with NFC Type 4 PR 4242
 - Improve error-checking and logging
 - Fix padding when already multiple of block_size
## 1.2
 - Move to GitHub
## 1.1
 - Add support for reading Seader files that have SIO
 - Add custom zero key ADF OID (0.3.1.7.9.0.0.0.0.0/030107090000000000)
 - Add persisting and reading of diversified keys, ADF response and ADF OID
## 1.0
 - Public release
 - Add native BLE
