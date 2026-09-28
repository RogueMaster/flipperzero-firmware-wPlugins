<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Credential validation

Use `<tiny_crypto/validation.h>` for certificate and CMS validation, and
`<tiny_crypto/credential.h>` for PIV and TWIC objects. These operations combine
signature checks, path discovery, certificate policy, and CRL evidence.
Enable `TINY_CRYPTO_ENABLE_CMS_VALIDATION` for the generic validation API.
PIV object and composed credential features remain optional.

## Set up storage

`TC_validation_capacity_init` provides three starting layouts. Each field is
adjustable before sizing the arena.

| Profile | Path certificates | CMS candidates | CRL records |
| :------ | ----------------: | -------------: | ----------: |
| Micro | 4 | 8 | 4 |
| Mini | 8 | 16 | 16 |
| Desktop | 16 | 128 | 128 |

Profiles select resource capacities. Build options select algorithms. Choose
capacities for your provisioned trust set and the credentials you accept; a
limit result requires an application decision about additional resources.

```c
#include <tiny_crypto/validation.h>

TC_result prepare_validation(TC_buffer arena,
    const TC_validation_trust* trust,
    const TC_validation_options* options,
    TC_validation_workspace* workspace,
    TC_validation_context* context)
{
    TC_validation_capacity capacity;
    TC_result status = TC_validation_capacity_init(TC_VALIDATION_MINI, &capacity);
    if (status != TC_RESULT_OK) return status;
    status = TC_validation_workspace_init(&capacity, arena, workspace);
    if (status != TC_RESULT_OK) return status;
    return TC_validation_context_init(trust, options, &workspace->credential, context);
}
```

Call `TC_validation_workspace_size` to obtain the required bytes.
`TC_validation_workspace_alignment` reports the alignment. An array of
`TC_validation_storage` provides suitable alignment for static storage; check
its byte capacity against the reported size. Typed-array workspace initializers
remain useful when each array has a fixed application-defined location.

Keep the workspace descriptor at the address used during initialization.
Place large arenas in static or application-owned memory on constrained devices.
One operation at a time may use an arena. Signature-provider scratch is separate.

## Configure policy and trust

`TC_validation_options` holds one evaluation time, signature provider, parsing
limits, and search bounds. Its `certificate` and `crl_signer` fields specify
separate usage, policy, and name constraints. Select CMS BER compatibility and
RSA parameter compatibility explicitly.

`TC_validation_trust` refers to a held certificate source and CRL index. Only
source anchors establish trust. Keep both sources unchanged until the acceptance
decision and all uses of their borrowed results finish.

Use separate contexts for card and content-signing trust when they have different
anchors or policies. Sequential contexts can share the same arena. Missing CRL
evidence prevents a valid result.

## Validate and reuse results

`TC_X509_validate` accepts certificate DER and returns the validated certificate,
evaluation time, and selected anchor index. The certificate's fields borrow DER.
They remain usable after another operation reuses the arena.

`TC_CMS_validate` checks one selected signer, attached or detached content, its
certificate path, and revocation. Application-specific object checks follow it.

`TC_PIV_CVC_validate` validates the X.509 signer and its CRLs, then verifies the
card's CVC chain. Secure-messaging key confirmation completes session setup.

For signed card objects:

1. Validate the card certificate and read its identifiers.
2. Pass the identifiers and card expiration to `TC_PIV_CHUID_validate`.
3. Use the returned CHUID and signer for biometric and Security Object requests.
4. Pass a successful `TC_PIV_security_result` to
   `TC_TWIC_unsigned_CHUID_validate` to check container 3002.

CHUID and Security Object results borrow original buffers and inventory
descriptors. Keep those buffers unchanged while using the results. Encrypted
biometric entries use their stored ciphertext for inventory hashes. TWIC printed
information can require decrypted TLVs; select the representation required by
the card profile and retain those bytes through validation. See
[inventory hash inputs](credential-validation.md#inventory-hash-inputs).

Share a remaining-work counter across the sequence. Accept only
`TC_CREDENTIAL_VALID`; handle invalid signatures, revocation, unavailable
evidence, unsupported algorithms, and exhausted limits explicitly.

## TWIC example

[credential_workflow.c](../examples/credential_workflow.c) composes public APIs over
caller-provided objects, held trust and CRL sources, a held CCL snapshot, and a
fresh card-proof callback. Card commands remain in the reader example.
The [composition walkthrough](credential-validation.md) documents its policy sequence,
borrowed lifetimes and final recheck.

The result covers the requested checks at the supplied evaluation time.
Recheck time-sensitive evidence before a later access decision. Release held
snapshots and wipe plaintext and key material on every exit path.
