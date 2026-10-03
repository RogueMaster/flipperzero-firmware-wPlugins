/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Validation context of examples/piv_inspect.c over caller trust anchors and
 * a CRL index, with the native signature provider. */
#ifndef EXAMPLE_PIV_INSPECT_TRUST_H_
#define EXAMPLE_PIV_INSPECT_TRUST_H_
#include "piv_inspect.h"
#include <tiny_crypto/x509_crypto.h>
#include <tiny_crypto/x509_store.h>

enum {
  EXAMPLE_PIV_TRUST_CERTIFICATE_BYTES = 16384,
  /* Certificates a signed card object may embed beside its signer, such as
   * the issuing CA of a TWIC content signer. */
  EXAMPLE_PIV_TRUST_EMBEDDED_CERTIFICATES = 4,
  EXAMPLE_PIV_TRUST_FRAMES = 32,
  EXAMPLE_PIV_TRUST_OIDS = 64,
  EXAMPLE_PIV_TRUST_ARENA_BYTES = 1024 * 1024
};

/* The context points into this storage, so keep it in place. */
typedef struct {
  TC_TLV_frame frames[EXAMPLE_PIV_TRUST_FRAMES];
  TC_bytes oids[EXAMPLE_PIV_TRUST_OIDS];
  TC_X509_workspace parser;
  TC_ECDSA_workspace ec;
  TC_RSA_word words[TC_RSA_VERIFY_WORKSPACE_WORDS(4096)];
  TC_RSA_workspace rsa;
  TC_X509_native_workspace native;
  TC_validation_storage arena[EXAMPLE_PIV_TRUST_ARENA_BYTES / sizeof(TC_validation_storage)];
  TC_validation_workspace workspace;
  TC_X509_store_anchor anchors[EXAMPLE_PIV_INSPECT_ANCHORS];
  TC_X509_store_array array;
  TC_X509_store_source source;
  TC_validation_options options;
  TC_validation_context context;
} ExamplePIVInspectTrust;

/* Build trust->context from the anchors, time and revocation policy of
 * options, with no CRLs. The anchors also serve as CRL signer candidates,
 * since a pinned issuing CA signs its own CRLs. Spans stay borrowed. Returns
 * 1 on success and 0 for a malformed anchor or too many of them. */
int example_piv_inspect_trust_init(ExamplePIVInspectTrust* trust,
                                   const ExamplePIVInspectOptions* options);

/* Rebuild trust->context with the CRLs of index, which stays borrowed and
 * unchanged while the context is used. Returns 1 on success. */
int example_piv_inspect_trust_revocation(ExamplePIVInspectTrust* trust,
                                         const TC_X509_crl_index* index);

#endif
