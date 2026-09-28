<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Composing PIV and TWIC validation

[`examples/credential_workflow.c`](../examples/credential_workflow.c) composes
the public validation APIs over retained PIV or TWIC credential objects, trust
snapshots, and policy. Card commands, response framing, and reader access belong
to the application.

## Inventory hash inputs

`TC_PIV_security_data.parts` selects the bytes authenticated for each container.
Choose this representation before checking a hash and keep it fixed for the
decision. A mismatch fails validation.

Encrypted biometric objects use their stored BC field, including its tag and
length. For a printed-plaintext policy, container `0x3001` uses the decrypted
printed-information TLVs. Keep that buffer stable until inventory validation
finishes, then wipe it. The outer GET DATA `53` wrapper is excluded in both cases.
The library hashes the supplied spans directly; it performs no decryption or
representation fallback during inventory validation.

In the reader utility, supply `--printed-plaintext`, `--security-object` and
`--tpk-hex` to hash the decrypted printed fields. The command keeps
the stored object and decrypted fields in separate buffers. Both remain in
locked memory until the final check finishes.

For TWIC NEXGEN, `example_credential_validate` also parses the authenticated DFC109
contents with `TC_PIV_printed_read`. It checks the field order and limits from
TWIC Part 2 section 4.7.2, including the eight-digit card serial and `7099`
issuer prefix. It then requires the `DDMMMYYYY` date to match the signed CHUID
expiration and remain current at the shared validation time. The accepted
result exposes borrowed printed fields through `result.printed` and sets
`result.has_printed`.

The shared sequence is:

1. Validate the selected card-key certificate under card-key trust.
2. Read and bind the certificate identifiers.
3. For TWIC, check the held canceled-card-list snapshot and its freshness metadata.
4. Ask the application to perform a fresh proof with the accepted public key.
5. Validate the signed CHUID under separate content-signer trust.
6. When supplied, validate the Security Object and its retained object inventory,
   then bind the unsigned CHUID to that accepted inventory.
7. When supplied, parse authenticated printed information and check its
   expiration against the signed CHUID.
8. When supplied, check each biometric format and authenticate every biometric
   object against the accepted CHUID.

The request selects PIV, TWIC Legacy, or TWIC NEXGEN and its signed CHUID
schema. `TC_CHUID_PROFILE_LEGACY_KEY_MAP` is the explicit PIV-shaped option for
the historical `3D` field. PIV uses strict PIV
identifier and OID rules. TWIC identity binding follows Part 3 section 4.4.4:
the signed certificate FASC-N identifies the credential. The certificate may
omit its UUID URI. A present UUID must satisfy the selected profile and match
the CHUID GUID; the complete FASC-N must match across both authenticated objects.
`TC_TWIC_card_identifiers_read` and `TC_TWIC_card_identifiers_match` implement
this reader policy. `TC_PIV_card_identifiers_read` performs the PIV profile check.
For TWIC, the workflow accepts the registered PIV or TWIC card-authentication
OID and passes the certificate's exact encoded OID into path validation. PIV
accepts the PIV OID.

`card_key` selects slot 9E Card Authentication or slot 9A PIV Authentication.
For slot 9A, the workflow reads the signed CHUID GUID before certificate
identifier selection. Strict PIV requires that Card UUID in the certificate.
A TWIC application can set `twic_reader_policy` to accept either registered
FASC-N OID and report an absent Card UUID while still checking any UUIDs that
are present. The proof callback receives the selected key reference so its
transport can address the correct slot.

`required_objects` states which evidence the application needs for its decision.
It can require the Security Object, unsigned CHUID, printed information, or each
biometric modality independently. Missing required evidence returns
`EXAMPLE_CREDENTIAL_UNAVAILABLE` before the card proof begins.

The two `TC_validation_context` values may share one initialized validation arena
because the operations are sequential. They use the same evaluation time. TWIC
also requires that time to equal `TC_TWIC_CCL_freshness_policy.now`; PIV leaves
the CCL and freshness fields zero. The proof callback receives
the selected profile and public key from the accepted card certificate. It owns
the transport and challenge exchange. It also receives the selected signature
scheme, hash, MGF hash, and salt length. The request chooses RSA v1.5 or PSS.
The wrapper enforces NEXGEN RSA-2048 and accepts Legacy RSA-1024 only when
`allow_legacy_rsa1024` is set explicitly.

The card context may provide an exact card-authentication purpose OID. With an
empty purpose, the example derives one exact PIV/TWIC-compatible purpose from
the certificate. It always requires digital-signature key usage, extended key
usage and an explicit purpose match before accepting the path.

All encoded inputs and trust sources are borrowed. Keep the certificate, CHUID,
object inventory, applicable CCL snapshot, and their backing storage immutable
until the acceptance decision is complete. `ExampleCredentialValidationResult`
retains borrowed views into those inputs. The example allocates no heap storage.

`example_credential_validate` returns a typed verdict for invalid credentials,
revocation, cancellation, stale data, unavailable evidence, unsupported
algorithms, exhausted limits and failed key possession. Treat
`EXAMPLE_CREDENTIAL_VALID` as authentication evidence. The application still
applies site authorization, live biometric matching and any required-object policy.

For a later access decision, initialize fresh card and content contexts with the
current application clock. TWIC also acquires the current CCL snapshot. Call
`example_credential_validate` again over the retained bytes to repeat the time,
trust, and cancellation checks that apply to the selected profile.
