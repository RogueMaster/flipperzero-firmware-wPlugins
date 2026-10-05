# Cryptography

Everything here is ICAO Doc 9303 part 11. The section numbers in the source
comments point at it, and every step that has a published test vector is
pinned to one in `tests/host` - if those pass, the implementation matches the
standard byte for byte, which is the only kind of assurance available without
a chip to try it on.

## Key derivation (section 9.7)

```
KDF(K, c) = H(K || c)      c is a 32 bit big-endian counter
```

`H` is SHA-1 for 3DES and AES-128 and SHA-256 for AES-192 and AES-256, and
the result is truncated to the key length. Counter 1 gives the encryption
key, counter 2 the MAC key, counter 3 the key that protects the PACE nonce.
For 3DES the odd parity bits of every DES byte are set afterwards, which is
cosmetic to the algorithm and load bearing to a chip that checks them.

*Pinned by* `tests/host/test_kdf.c`: appendix D.1 for the 3DES keys from
`Kseed`, D.3 for the session keys from `K.IFD xor K.IC`, G.1 for the AES-128
session keys and for `Kpi`.

## BAC (section 4.3, appendix D)

```
  MRZ information  = DocNumber || cd || DateOfBirth || cd || DateOfExpiry || cd
  Kseed            = SHA-1(MRZ information) truncated to 16 bytes
  K_Enc, K_MAC     = KDF(Kseed, 1), KDF(Kseed, 2)

  -> GET CHALLENGE                     <- RND.IC
     S      = RND.IFD || RND.IC || K.IFD
     E.IFD  = 3DES-CBC(K_Enc, S), zero IV
     M.IFD  = Retail MAC(K_MAC, E.IFD)
  -> EXTERNAL AUTHENTICATE E.IFD || M.IFD   <- E.IC || M.IC

     verify M.IC, then decrypt, then check RND.IFD came back unchanged
     Kseed' = K.IFD xor K.IC
     KS_Enc, KS_MAC = KDF(Kseed', 1), KDF(Kseed', 2)
     SSC    = RND.IC[4..8] || RND.IFD[4..8]
```

The order in the middle is not a matter of taste: the MAC is verified before
the ciphertext is touched, and the random value is compared after. A chip
that returns the wrong `RND.IFD` has answered a different challenge, which
means the MRZ input was wrong rather than the document being faulty - and
that is the difference between `EmrtdErrorWrongKey` and
`EmrtdErrorSecureMessaging` on the screen.

`emrtd_bac_build_external_auth()` takes `RND.IFD` and `K.IFD` as arguments
rather than drawing them internally, so that the appendix D vectors can pin
the whole exchange; passing NULL draws them from the hardware generator.

*Pinned by* `tests/host/test_bac.c` against appendix D.3, the worked
authentication and session key establishment, and by `test_mac.c` for the
Retail MAC inside it.

Document numbers longer than nine characters are the awkward case: the check
digit is computed over the whole number rather than over a nine character
field, which is the extended TD1 and TD2 form of appendix D.2.
`emrtd_mrz_information()` handles it and `test_mrz.c` checks it.

## PACE, generic mapping over ECDH (section 4.4, appendix G)

```
  Kpi = KDF(f(password), 3)         f(MRZ) = SHA-1(MRZ information)
                                    f(CAN) = the CAN characters as they are

  -> MSE:Set AT   the PACE OID and which password is in use
  -> GA           encrypted nonce      <- z ;  s = D(Kpi, z)
  -> GA           map nonce  PK.IFD    <- PK.IC
                  H = ephemeral ECDH shared point
                  G^ = s * G + H                    the mapped generator
  -> GA           key agreement over G^             <- PK'.IC
                  K = x coordinate of the shared point
                  KS_Enc, KS_MAC = KDF(K, 1), KDF(K, 2)
  -> GA           T.IFD = CMAC(KS_MAC, 7F49 || 06 oid || 86 PK'.IC)
                  <- T.IC, checked the same way over PK'.IFD
     SSC = 0
```

Both tokens are computed over the *other* side's public point, which is what
makes the exchange mutual: a chip that cannot produce `T.IC` did not derive
the same key, and the only way to derive the same key is to know the
password.

Two checks are worth naming because skipping them is the classic way to get
this wrong:

- every point received is read with `emrtd_ec_point_read()`, which verifies
  that it lies on the curve **before** it is multiplied by anything. A point
  off the curve, or in a small subgroup, would leak the ephemeral scalar;
- the ephemeral scalar itself comes from `mbedtls_ecp_gen_privkey()`, which
  produces a value in the right range for the group without a reject loop
  (item 8 of [platform.md](platform.md)).

*Pinned by* `tests/host/test_ec.c` - the generic mapping on brainpoolP256r1
from appendix G.2 and the key agreement over the mapped generator from G.3 -
and by `test_pace.c` for `Kpi`, the token input and the whole exchange
against a simulated chip.

### Curves

The parameter id in `PACEInfo` selects a curve from BSI TR-03110 part 3
table 4:

| id | Curve | This build |
| --- | --- | --- |
| 8 | NIST P-192 | yes |
| 9 | brainpoolP192r1 | yes |
| 10 | NIST P-224 | yes |
| 11 | brainpoolP224r1 | yes |
| 12 | NIST P-256 | yes |
| 13 | brainpoolP256r1 | yes, and this is what most European passports use |
| 14-18 | brainpoolP320r1, NIST P-384, brainpoolP384r1, brainpoolP512r1, NIST P-521 | no - above `MBEDTLS_ECP_MAX_BITS` |
| 0-2 | MODP groups for PACE over DH | no - modular exponentiation cannot be linked |

The brainpool curves are not compiled into the firmware's mbed TLS, so their
domain parameters are populated by hand into an `mbedtls_ecp_group`. That is
supported for short Weierstrass curves: with `grp->modp` left NULL the
generic reduction is used, and with the cofactor left at zero
`mbedtls_ecp_group_free()` releases the parameters instead of treating the
group as one of the library's own constants. The whole story, including what
was tried and failed, is in [platform.md](platform.md).

An unusable curve is still in the table. A reader that says "brainpoolP384r1
is above the 256 bit limit of this build" is telling the user something;
"PACE failed" is not.

## Secure Messaging (section 9.8)

| | 3DES | AES |
| --- | --- | --- |
| Encryption | CBC, zero IV | CBC, IV = E(KS_Enc, SSC) |
| Checksum | Retail MAC, ISO 9797-1 algorithm 3 | CMAC, NIST SP 800-38B |
| Truncated to | 8 bytes | 8 bytes |
| Block, and the size of the SSC | 8 | 16 |

The MAC input is `SSC || M`, padded with **ISO 9797-1 method 2** - one `0x80`
byte and then zeroes - for *both* families. For 3DES that is what the
appendix D vectors show. For AES the padding is not needed by CMAC itself,
which pads internally, and yet a real passport expects it there; it is the
detail most likely to produce a session that authenticates and then fails on
the first real command.

The AES initialisation vector is derived per command by encrypting the send
sequence counter with the encryption key in ECB mode, so two identical
commands never produce the same ciphertext.

*Pinned by* `tests/host/test_sm.c` against the protected APDUs of appendix
D.4, and by `test_mac.c` against RFC 4493 for AES-128 CMAC and NIST SP
800-38B D.2 and D.3 for AES-192 and AES-256 - the latter two because the
firmware does not ship `MBEDTLS_CMAC_C` and the implementation here is built
on `mbedtls_aes_crypt_ecb()`.

## Random numbers

`emrtd_random_fill()` is the hardware generator on the device and the
operating system generator in the host build. The vector tests inject fixed
values instead, which is the only way to reproduce a published exchange.
`emrtd_random_mbedtls()` exists because `mbedtls_ecp_mul()` refuses a NULL
generator: it randomises its intermediate values as a side channel defence,
and that is worth keeping.

## Passive authentication, and what is not done here

EF.SOD is parsed far enough to read the digest algorithm and the list of data
group hashes, and every group that is read is hashed and compared with its
entry. A mismatch is reported per file, as is a group that EF.SOD does not
list at all.

The signature over EF.SOD is **not** verified on the device. It is an RSA or
ECDSA signature by a document signer certificate, which in turn is signed by
a country signing certificate, and the chain has to be checked against a list
of trusted country certificates that the device has no way to obtain or keep
current. `MBEDTLS_RSA_C` is off in the firmware's build in any case. The
consequence is stated plainly in the README and worth repeating: matching
hashes prove the files on the chip belong together, not that the chip is
genuine.

## Key material in memory

Session keys live in an `EmrtdSm`, a member of the heap allocated worker, and
are wiped by `emrtd_sm_clear()` when the session ends. A copy of `KSenc`,
`KSmac` and the counter the session opened with is taken at that opening and
kept in the read result, so that `Result -> Keys` can show the holder the keys
their own document produced; it lives until the application closes or the next
read starts. Neither copy is ever written to the export or the trace. The
credentials live for the life of the application too, and are on the SD card
unless **Remember on SD** has been turned off; [security.md](security.md)
applies to both.
