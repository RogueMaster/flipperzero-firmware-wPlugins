/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Source-backed CRLs of examples/piv_inspect.c: each CRL source is scanned
 * once for the CRL targets of an inventory (TC_PIV_card_crl_targets), and the
 * prepared records form the CRL index of the trust context. A CRL of any
 * size needs only the fixed storage below. */
#ifndef EXAMPLE_PIV_INSPECT_CRL_H_
#define EXAMPLE_PIV_INSPECT_CRL_H_
#include "piv_inspect.h"
#include "piv_inspect_trust.h"
#include <tiny_crypto/x509_crl_source.h>

enum {
  /* Card certificates, signers and embedded issuers of one card. */
  EXAMPLE_PIV_CRL_TARGETS = 32,
  EXAMPLE_PIV_CRL_JOB_UNITS = 256,
  EXAMPLE_PIV_CRL_METADATA_BYTES = 4096,
  EXAMPLE_PIV_CRL_WINDOW_BYTES = 4096,
  EXAMPLE_PIV_CRL_CERTIFICATE_BYTES = 16384
};

/* The jobs own the prepared records, so keep this storage in place while
 * the trust context uses the index. */
typedef struct {
  TC_X509_crl_target targets[EXAMPLE_PIV_CRL_TARGETS];
  uint8_t certificates[EXAMPLE_PIV_CRL_CERTIFICATE_BYTES];
  TC_PIV_crl_target_workspace target_workspace;
  TC_X509_crl_storage state[EXAMPLE_PIV_INSPECT_CRLS][EXAMPLE_PIV_CRL_JOB_UNITS];
  TC_X509_crl_match matches[EXAMPLE_PIV_INSPECT_CRLS][EXAMPLE_PIV_CRL_TARGETS];
  uint8_t metadata[EXAMPLE_PIV_INSPECT_CRLS][EXAMPLE_PIV_CRL_METADATA_BYTES];
  uint8_t window[EXAMPLE_PIV_CRL_WINDOW_BYTES];
  uint8_t entry[EXAMPLE_PIV_CRL_METADATA_BYTES], issuer[EXAMPLE_PIV_CRL_METADATA_BYTES];
  TC_X509_crl_job* jobs[EXAMPLE_PIV_INSPECT_CRLS];
  TC_X509_crl_record records[EXAMPLE_PIV_INSPECT_CRLS];
  TC_X509_crl_index index;
  size_t target_count;
} ExamplePIVInspectCRLs;

/* Prepare every CRL source of options for the CRL targets of inventory and
 * install the index in trust. Earlier records are cleared first. The parsing
 * limits and name workspace come from trust. A source that cannot be read,
 * or a CRL that does not parse, returns 0 with no CRLs installed. */
int example_piv_inspect_crls_prepare(ExamplePIVInspectCRLs* crls,
                                     const ExamplePIVInspectOptions* options,
                                     const TC_PIV_inventory* inventory,
                                     ExamplePIVInspectTrust* trust);

/* Wipe the jobs and every record borrowed from them. */
void example_piv_inspect_crls_clear(ExamplePIVInspectCRLs* crls);

#endif
