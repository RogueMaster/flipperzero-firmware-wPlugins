/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_inspect_crl.h"
#include <string.h>

enum {
  TARGET_WORK = 200000000,
  /* Per-step bounds: work, entries and hashed bytes. */
  STEP_WORK = 1000000,
  STEP_ENTRIES = 256,
  STEP_BYTES = 65536
};

void example_piv_inspect_crls_clear(ExamplePIVInspectCRLs* crls)
{
  for (size_t i = 0; i < EXAMPLE_PIV_INSPECT_CRLS; ++i)
    TC_X509_crl_prepare_clear(crls->jobs[i]);
  TC_secure_zero(crls, sizeof *crls);
}

/* Scan one CRL source for the targets into record. */
static int crl_prepare(ExamplePIVInspectCRLs* crls, size_t i, const TC_source* source,
                       const ExamplePIVInspectTrust* trust)
{
  const uint64_t length = source->length;
  /* The whole CRL, read in window-sized pieces: every byte at most a few
   * times, and at most one entry per two bytes. */
  const TC_X509_crl_prepare_options options = {
      trust->options.parsing, length, length * 3 + EXAMPLE_PIV_CRL_WINDOW_BYTES,
      length + EXAMPLE_PIV_CRL_METADATA_BYTES, length / 2 + 1};
  const TC_X509_crl_prepare_workspace workspace = {
      {(uint8_t*)crls->state[i], sizeof crls->state[i]},
      {crls->window, sizeof crls->window},
      {crls->metadata[i], sizeof crls->metadata[i]},
      {crls->entry, sizeof crls->entry},
      {crls->issuer, sizeof crls->issuer},
      trust->parser,
      trust->workspace.path.validation.names,
      crls->matches[i],
      EXAMPLE_PIV_CRL_TARGETS};
  size_t work = STEP_WORK;
  TC_TLV_result status = TC_X509_crl_prepare_begin(source, crls->targets, crls->target_count,
                                                   &options, &workspace, &work, &crls->jobs[i]);
  int complete = 0;
  while (status == TC_TLV_OK && !complete) {
    work = STEP_WORK;
    status = TC_X509_crl_prepare_step(crls->jobs[i], STEP_ENTRIES, STEP_BYTES, &work, &complete);
  }
  if (status == TC_TLV_OK)
    status = TC_X509_crl_prepare_finish(crls->jobs[i], &crls->records[i]);
  return status == TC_TLV_OK;
}

int example_piv_inspect_crls_prepare(ExamplePIVInspectCRLs* crls,
                                     const ExamplePIVInspectOptions* options,
                                     const TC_PIV_inventory* inventory,
                                     ExamplePIVInspectTrust* trust)
{
  static const TC_X509_crl_index no_crls = {NULL, 0, 0};
  if (options->crl_count > EXAMPLE_PIV_INSPECT_CRLS ||
      !example_piv_inspect_trust_revocation(trust, &no_crls))
    return 0;
  example_piv_inspect_crls_clear(crls);
  crls->target_workspace.certificates = (TC_buffer){crls->certificates, sizeof crls->certificates};
  crls->target_workspace.parsing = trust->parser;
  size_t work = TARGET_WORK;
  if (TC_PIV_card_crl_targets(inventory, &trust->options.parsing, &crls->target_workspace, &work,
                              crls->targets, EXAMPLE_PIV_CRL_TARGETS,
                              &crls->target_count) != TC_PIV_OK)
    return 0;
  for (size_t i = 0; i < options->crl_count; ++i)
    if (!crl_prepare(crls, i, &options->crls[i], trust)) {
      example_piv_inspect_crls_clear(crls);
      return 0;
    }
  crls->index = (TC_X509_crl_index){crls->records, options->crl_count, 0};
  return example_piv_inspect_trust_revocation(trust, &crls->index);
}
