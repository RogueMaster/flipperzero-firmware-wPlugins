/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* RFC 5914 TrustAnchorList reader.
 * Standards: RFC 5914, RFC 5937.
 * Configuration: TC_ENABLE_TRUST_ANCHOR_FORMAT and the TC_TAF_ENABLE_*
 * choices.
 * Contracts: docs/api.md. Guide: docs/x509-trust-anchors.md. */
#ifndef TINY_CRYPTO_X509_TRUST_ANCHOR_H_
#define TINY_CRYPTO_X509_TRUST_ANCHOR_H_
#include <tiny_crypto/x509_store.h>
#ifdef __cplusplus
extern "C" {
#endif

/* RFC 5914 TrustAnchorList reader. init binds the list, limits and workspace.
 * Treat members as private after init. The reader and decoded records borrow
 * the DER. Keep it unchanged through validation. The caller owns the
 * workspace frames and extension OID scratch. Keep them alive and exclusive
 * to this reader until the last next call. */
typedef struct {
  TC_TLV_reader reader;
  const TC_X509_workspace* workspace;
} TC_X509_trust_anchor_reader;

#if TC_ENABLE_TRUST_ANCHOR_FORMAT
/* Bind a DER TrustAnchorList, a SEQUENCE of one or more TrustAnchorChoice
 * values (RFC 5914 section 4), to reader. init walks the complete list under
 * limits, so every anchor fits them, and limits also apply to each anchor
 * that next decodes. frames need one entry per constructed nesting level of
 * the list. The reader, the frames and the extension OID array are written,
 * so they must be pairwise disjoint and lie outside encoded and the
 * workspace struct. Charges no work.
 * Returns OK with reader initialized. ARGUMENT for NULL arguments, a
 * workspace array that is NULL with a capacity, or overlap. INVALID for a
 * malformed or empty list. LIMIT when limits or frames are exhausted. reader
 * changes only on OK. */
TC_TLV_result TC_X509_trust_anchor_list_init(TC_X509_trust_anchor_reader* reader, TC_bytes encoded,
                                             const TC_TLV_limits* limits,
                                             const TC_X509_workspace* workspace);
/* Decode the next TrustAnchorChoice (RFC 5914 section 2): a Certificate, a
 * TBSCertificate or a TrustAnchorInfo, each into a TC_X509_store_anchor with
 * normalized path controls (RFC 5937 section 2). A TrustAnchorInfo without
 * certPath is returned with x509_unusable set. Its public key remains
 * available for other purposes. out borrows the list DER. out must lie
 * outside the list, the reader, the workspace struct and both workspace
 * arrays. Charges no work.
 * Returns OK with the reader advanced and out written. END when no anchor
 * remains. ARGUMENT for a NULL argument or overlap. UNSUPPORTED for a CHOICE
 * variant disabled by its TC_TAF_ENABLE_* option or an unsupported version.
 * INVALID for a malformed anchor, and LIMIT for exhausted limits or
 * workspace capacity. The reader and out change only on OK. Workspace
 * scratch may change on failure. */
TC_TLV_result TC_X509_trust_anchor_next(TC_X509_trust_anchor_reader* reader,
                                        TC_X509_store_anchor* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
