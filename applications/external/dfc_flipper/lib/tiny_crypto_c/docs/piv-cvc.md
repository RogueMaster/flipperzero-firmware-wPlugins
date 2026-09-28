<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# PIV secure-messaging CVCs

Include `<tiny_crypto/piv_cvc.h>`. `TC_PIV_CVC_read` reads the complete `7F21`
container and returns borrowed fields, including the original signed TLVs.
`TC_PIV_CVC_chain_verify` verifies a card CVC and an optional intermediate using
a previously validated content-signing certificate.

Validate that certificate's trust path, content-signing usage, certificate policy,
time and revocation first. Hold the certificate bytes and trust inputs stable
through the operation. [X.509 validation](x509-path.md) describes path validation;
[CMS credential validation](cms.md) describes the content-signing workflows.

The chain verifier applies SP 800-73-5 Part 2, section 4.1.5:

- A direct card CVC identifies its signer with the first eight bytes of the
  certificate's subjectKeyIdentifier.
- An intermediate uses that same issuer link. Its subject identifier is the
  first eight bytes of SHA-1 over its public-key object, `04 || X || Y`.
- The card CVC identifies its intermediate by that intermediate's subject ID.
  The intermediate signature uses RSA/SHA-256; card signatures use ECDSA/SHA-256
  for CS2 or ECDSA/SHA-384 for CS7.

The selected curve must match the CVC public keys. Both points are checked for
curve membership. Supply a known card UUID to bind the subject to that card,
or an empty span to obtain its identifier from the verified CVC. Secure-messaging
key confirmation must succeed before accepting the card session.

## Calling the verifier

Enable `TINY_CRYPTO_ENABLE_PIV_CVC`, `TINY_CRYPTO_ENABLE_X509`,
`TINY_CRYPTO_ENABLE_EC` and the selected curve. An intermediate also requires
`TINY_CRYPTO_ENABLE_SHA1`. Configure the signature provider for the required
RSA/ECDSA and SHA algorithms.

```c
#include <tiny_crypto/piv_cvc.h>

TC_X509_signature_result verify_card_cvc(
    TC_bytes encoded, TC_bytes intermediate, TC_bytes expected_uuid,
    TC_EC_curve curve, const TC_X509_certificate* validated_signer,
    const TC_X509_signature_provider* provider,
    TC_EC_workspace* point_scratch, size_t* work, TC_PIV_CVC* result)
{
    const TC_TLV_limits limits = {4096, 4096, 128, 16};
    const TC_PIV_CVC_chain_request request = {
        encoded, intermediate, expected_uuid, curve, validated_signer
    };
    return TC_PIV_CVC_chain_verify(&request, &limits, provider,
        point_scratch, work, result);
}
```

Allocate `TC_EC_workspace` outside small task stacks and separately from the
signature provider's scratch. Inputs, result, work and point scratch must be
disjoint. The verifier retains no storage and clears point scratch after use.
On `TC_X509_SIGNATURE_VALID`, the result borrows the card CVC's original bytes.
All other outcomes preserve the result. Handle `INVALID`, `UNSUPPORTED`, `LIMIT`
and `ERROR` explicitly; each ends this validation attempt.

## Signer trust and revocation

`TC_PIV_CVC_validate` in `<tiny_crypto/credential.h>` combines signer path
discovery, content-signing policy, revocation and CVC verification through a
`TC_validation_context`. `example_validate_cvc` adapts the focused example's
older path and CRL policy inputs to that public operation.

For PIV, the helper requires `id-fpki-common-piv-contentSigning`, digitalSignature
key usage and the content-signing EKU. The signer must be valid at the evaluation
time. Selecting a TWIC profile accepts the registered PIV/TWIC content-signing
purposes and retains the caller's certificate-policy settings. The certificate
and CRL policies must use the same evaluation time.

Both operations return `TC_credential_status`, shared with CMS credential validation.
Only `TC_CREDENTIAL_VALID` writes the borrowed CVC result. Revoked, unavailable,
unsupported, invalid, limit and API-error outcomes remain distinct. Path and
point checks can share scratch across sequential phases. The example uses the
micro validation capacity with four certificate and four CRL slots, then clears
its workspace before returning.

Hold the snapshot, issuer candidates, anchors, CRLs and credential bytes stable
through the acceptance decision. Decode the GENERAL AUTHENTICATE response in the
application layer, preserving the exact CVC bytes. `TC_PIV_SM_authenticate_response`
accepts those decoded peer fields and the validated signer, verifies the response
CVC chain, and completes key confirmation. Its point and session scratch share a
caller-owned union because the phases run sequentially. EAC certificates use their
own profile. Include `tiny_crypto/piv_sm_authenticate.h` for this combined helper;
`tiny_crypto/piv_sm.h` remains usable without the CVC and X.509 modules.
