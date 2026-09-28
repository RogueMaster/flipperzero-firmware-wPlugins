<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Native signature verification

`<tiny_crypto/x509_crypto.h>` supplies a native signature provider for the
X.509 message, digest, certificate, and path APIs. Enable X.509, the required EC/RSA
algorithms, and the hashes used by your certificates.

```c
TC_ECDSA_workspace ec;
TC_RSA_word words[TC_RSA_VERIFY_WORKSPACE_WORDS(3072)];
TC_RSA_workspace rsa = {words, sizeof words / sizeof *words};
TC_X509_native_workspace scratch = {&ec, &rsa, TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK};
TC_X509_signature_provider provider = TC_X509_native_provider(&scratch);
```

Pass `provider` to `TC_X509_signature_verify`, or assign it to the path
options' `signatures` field. Set an unused EC or RSA workspace pointer to
NULL. Keep the workspace metadata alive for every call and serialize calls
that share scratch. Neither scratch nor metadata may overlap input or the
work counter.

The provider hashes message segments in order without copying their contents.
It supports DER ECDSA signatures and RSA PKCS#1 v1.5/PSS. PSS-only keys cannot
verify v1.5 signatures. When PSS key parameters are present, signature hash
and MGF hash must match them, and the salt length must meet the key's minimum.

For content already hashed, use `TC_X509_signature_verify_digest` with a
`TC_signature_algorithm`. Specify the signature scheme, digest hash, and, for
PSS, its MGF hash and salt length in bytes. The function verifies the digest
directly, not a hash of the digest. PSS still needs its signature and MGF hash
implementations to check the encoding. ECDSA and v1.5 can use externally computed
hashes without enabling the corresponding hash implementation.

Custom providers can supply `verify`, `verify_digest`, or both. Initialize unused
callbacks to NULL. A missing operation returns `UNSUPPORTED`; digest verification
never falls back to the message callback. Both callbacks must enforce key
restrictions, perform cryptographic verification, and never increase `work`.

`signature_work` is reserved from the shared work budget for each crypto
attempt. Parsing, input checks and hashing also consume work. The value
`TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK` accommodates supported curves and RSA
sizes with ordinary public exponents. This is an operation budget, not a time measurement.

A valid signature does not establish certificate trust. Path validation and
application rules still determine trusted issuers, permitted uses, validity,
and accepted algorithms. Missing compiled algorithms return `UNSUPPORTED`.

With OpenSSL tests enabled, run the provider's comparison tests with:

```sh
ctest --test-dir build -R '^test_x509_(native|signature)$' --output-on-failure
```
