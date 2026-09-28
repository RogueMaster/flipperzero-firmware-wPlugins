<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# TWIC barcodes and privacy keys

Enable `TINY_CRYPTO_ENABLE_AAMVA` and `TINY_CRYPTO_ENABLE_TWIC_TPK`, then include
`<tiny_crypto/aamva.h>` and `<tiny_crypto/twic_tpk.h>`. TPK container parsing
also requires `TINY_CRYPTO_ENABLE_TLV`. Supply the raw bytes produced by a
barcode decoder.
Image loading and PDF417 recognition belong in the application.

Call `TC_AAMVA_subfile_find` with `"ZT"`, then `TC_AAMVA_field_find` with `"ZTA"`
on the returned subfile. Both functions return borrowed spans. Keep the barcode
buffer alive and unchanged until processing finishes. `TC_TLV_END` means the
requested subfile or field is absent. Duplicate requested fields are invalid.

Pass the ZTA value to `TC_TWIC_tpk_read` with `TC_TWIC_TPK_BARCODE_HEX`. This mode
accepts hexadecimal DFC101 containers and the DCF101 prefix shown in TWIC's
barcode example. The decoder checks the C0 key, C1 algorithm and C2 key-index
fields before returning an AES-128 key. Applications that remove the outer
container can pass the C0/C1/C2 fields with `TC_TWIC_TPK_CONTENTS`.
Card containers use `TC_TWIC_TPK_CARD`
and require DFC101. Strip transport framing before passing a card container.

The TPK result owns its 16 key bytes. Wipe it with
`TC_secure_zero(&key, sizeof key)`
after use, along with any application buffers containing the barcode or decoded
objects. These functions preserve output on errors. Output storage must be
separate from input buffers.

The AAMVA reader checks ANSI directory framing and printable-ASCII text fields.
It accepts an optional LF after a subfile designator and empty field values.
It leaves date, name and jurisdiction-specific interpretation to the caller.
Version 00 headers return `TC_TLV_UNSUPPORTED`; historical-version conformance
and non-ASCII field decoding have separate requirements.

A decoded barcode supplies key material. Authenticate the decrypted object's
signature and signer path before using its identity or biometric data.

`TC_TWIC_object_decrypt` decrypts an enciphered object's `BC` value in place.
Enable `TINY_CRYPTO_ENABLE_TWIC_OBJECT_CRYPTO`, AES, ECB mode, and a 128-bit AES
key configuration. Object encryption has no dependency on the TPK container reader.
Supply a nonempty block-aligned buffer and separate output-length storage.
The function checks PKCS#7 padding across the final block, wipes removed padding,
and returns the plaintext length. Invalid padding or a processing error wipes
the whole buffer. Bad arguments preserve it. Treat recovered content as
untrusted until its signature and credential checks succeed.

`TC_TWIC_object_encrypt` pads and encrypts an object in place. Reserve up to
`TC_AES_BLOCKLEN` extra bytes, including a full padding block for aligned input.
Pass the plaintext length and buffer capacity separately; the output length
includes padding. Empty plaintext is supported. Bad arguments preserve the
buffer and output length. Processing failures wipe the padded region.
For signed object types, build and sign the object before encrypting it.
TWIC enciphered printed information (`DFC109`) has no signature block; its
decrypted fields do not provide an authenticated identity.

With the runtime S-box profile, call `TC_AES_init_sbox()` once during startup,
before any TWIC encryption or decryption. Complete initialization before starting
concurrent crypto operations. An uninitialized S-box produces a processing error.

The wire formats are described in AAMVA's DL/ID Card Design Standard, Annex D,
and TSA's TWIC NEXGEN & Legacy Part 2 v5, sections 4.6.2 and 4.9.
