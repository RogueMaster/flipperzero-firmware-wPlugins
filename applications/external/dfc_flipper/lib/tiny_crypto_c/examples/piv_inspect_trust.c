/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_inspect_trust.h"
#include <string.h>

int example_piv_inspect_trust_init(ExamplePIVInspectTrust* trust,
                                   const ExamplePIVInspectOptions* options)
{
  if (!options->anchor_count || options->anchor_count > EXAMPLE_PIV_INSPECT_ANCHORS)
    return 0;
  const TC_TLV_limits limits = {EXAMPLE_PIV_TRUST_CERTIFICATE_BYTES,
                                EXAMPLE_PIV_TRUST_CERTIFICATE_BYTES, 512, EXAMPLE_PIV_TRUST_FRAMES};
  trust->parser = (TC_X509_workspace){
      {trust->frames, EXAMPLE_PIV_TRUST_FRAMES}, trust->oids, EXAMPLE_PIV_TRUST_OIDS};
  trust->rsa = (TC_RSA_workspace){trust->words, sizeof trust->words / sizeof *trust->words};
  trust->native =
      (TC_X509_native_workspace){&trust->ec, &trust->rsa, TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK};
  TC_validation_capacity capacity;
  if (TC_validation_capacity_init(TC_VALIDATION_DESKTOP, &capacity) != TC_RESULT_OK ||
      TC_validation_workspace_init(&capacity,
                                   (TC_buffer){(uint8_t*)trust->arena, sizeof trust->arena},
                                   &trust->workspace) != TC_RESULT_OK)
    return 0;
  for (size_t i = 0; i < options->anchor_count; ++i) {
    TC_X509_certificate view;
    if (TC_X509_read(options->anchors[i], &limits, &trust->parser, &view) != TC_TLV_OK ||
        TC_X509_store_anchor_from_certificate(&view, &limits, &trust->parser, &trust->anchors[i]) !=
            TC_TLV_OK)
      return 0;
  }
  /* The anchor certificates are also the candidates of CRL signer searches. */
  trust->array = (TC_X509_store_array){options->anchors, options->anchor_count, trust->anchors,
                                       options->anchor_count};
  if (TC_X509_store_array_source(&trust->array, &trust->source) != TC_TLV_OK)
    return 0;
  memset(&trust->options, 0, sizeof trust->options);
  trust->options.at = options->at;
  trust->options.signatures = TC_X509_native_provider(&trust->native);
  trust->options.parsing = limits;
  trust->options.max_certificates = 8;
  trust->options.max_input = 8 * EXAMPLE_PIV_TRUST_CERTIFICATE_BYTES;
  /* A path search examines the anchors and the certificates of the CMS
   * object it validates. */
  trust->options.max_candidates =
      EXAMPLE_PIV_INSPECT_ANCHORS + EXAMPLE_PIV_TRUST_EMBEDDED_CERTIFICATES;
  trust->options.max_candidate_bytes =
      trust->options.max_candidates * EXAMPLE_PIV_TRUST_CERTIFICATE_BYTES;
  trust->options.revocation = options->revocation;
  static const TC_X509_crl_index no_crls = {NULL, 0, 0};
  return example_piv_inspect_trust_revocation(trust, &no_crls);
}

int example_piv_inspect_trust_revocation(ExamplePIVInspectTrust* trust,
                                         const TC_X509_crl_index* index)
{
  const TC_validation_trust sources = {&trust->source, index};
  return TC_validation_context_init(&sources, &trust->options, &trust->workspace.credential,
                                    &trust->context) == TC_RESULT_OK;
}
