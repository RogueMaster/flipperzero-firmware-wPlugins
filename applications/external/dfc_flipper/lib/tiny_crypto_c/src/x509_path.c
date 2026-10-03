/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_PATH
#include "x509_path_internal.h"
#include "pki_status_internal.h"
#include "pki_internal.h"
#include "pki_storage_internal.h"
#include "pki_extensions_internal.h"
#include "x509_time_internal.h"
#include "internal.h"

int tc_x509_path_source_valid(const tc_x509_path_input* input)
{
  return input && input->summaries &&
         (input->certificates || (input->encoded && input->parser && input->cache));
}

static TC_bytes path_encoded(const tc_x509_path_input* input, size_t index)
{
  return input->certificates ? input->certificates[index].encoded : input->encoded[index];
}

TC_TLV_result tc_x509_path_certificate(const tc_x509_path_input* input, size_t index, size_t* work,
                                       const TC_X509_certificate** certificate)
{
  TC_bytes encoded;
  TC_TLV_result result;
  if (input->certificates) {
    *certificate = &input->certificates[index];
    return TC_TLV_OK;
  }
  encoded = input->encoded[index];
  if (tc_pki_work_charge(work, encoded.length) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = TC_X509_read(encoded, input->limits, input->parser, &input->cache[index]);
  if (result != TC_TLV_OK)
    return result;
  *certificate = &input->cache[index];
  return TC_TLV_OK;
}

/* Summary of path entry index, filled from certificate when not ready. The
 * basic pass supplies the view it already holds, so filling the cache parses
 * each certificate once. */
static TC_TLV_result path_summary_of(const tc_x509_path_input* input, size_t index,
                                     const TC_X509_certificate* certificate,
                                     const TC_X509_name_workspace* names, size_t* work,
                                     const TC_X509_extension_summary** out)
{
  TC_X509_extension_summary* summary = &input->summaries[index];
  if (!summary->ready) {
    TC_X509_extension_summary filled;
    int self_issued = 0;
    TC_TLV_result result = tc_x509_extensions_summarize(certificate, input->limits, work, &filled);
    /* Path length, name constraints and policy counters treat self-issued
     * intermediates specially (RFC 5280 section 6.1). */
    if (result == TC_TLV_OK && index + 1 < input->count)
      result = TC_X509_name_equal(certificate->subject, certificate->issuer, input->limits, names,
                                  work, &self_issued);
    if (result != TC_TLV_OK)
      return result;
    filled.self_issued = (uint8_t)self_issued;
    *summary = filled;
  }
  *out = summary;
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_path_summary(const tc_x509_path_input* input, size_t index,
                                   const TC_X509_name_workspace* names, size_t* work,
                                   const TC_X509_extension_summary** out)
{
  const TC_X509_certificate* certificate = NULL;
  if (!input->summaries[index].ready) {
    TC_TLV_result result = tc_x509_path_certificate(input, index, work, &certificate);
    if (result != TC_TLV_OK)
      return result;
  }
  return path_summary_of(input, index, certificate, names, work, out);
}

TC_TLV_result tc_x509_path_basic(const tc_x509_path_input* input,
                                 const TC_X509_name_workspace* workspace, size_t* work,
                                 int* accepted)
{
  size_t i, j, total = 0, remaining, anchor_remaining;
  TC_bytes issuer_name;
  TC_X509_public_key issuer_key;
  if (!tc_x509_path_source_valid(input) || !workspace || !work || !accepted || !input->anchor ||
      !input->at || !input->limits || !input->count)
    return TC_TLV_ARGUMENT;
  anchor_remaining = input->anchor_path_len;
  if (input->count > input->max_certificates)
    return TC_TLV_LIMIT;
  for (i = 0; i < input->count; ++i) {
    const TC_bytes encoded = path_encoded(input, i);
    if (!encoded.data || !encoded.length)
      return TC_TLV_ARGUMENT;
    if (encoded.length > input->max_input - total)
      return TC_TLV_LIMIT;
    total += encoded.length;
    if (tc_pki_work_charge(work, encoded.length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    for (j = 0; j < i; ++j) {
      const TC_bytes previous = path_encoded(input, j);
      if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      if (encoded.length == previous.length) {
        if (tc_pki_work_charge(work, encoded.length) != TC_TLV_OK)
          return TC_TLV_LIMIT;
        if (tc_pki_equal(encoded, previous)) {
          *accepted = 0;
          return TC_TLV_OK;
        }
      }
    }
  }
  remaining = input->count - 1;
  issuer_name = input->anchor->name;
  issuer_key = input->anchor->public_key;
  for (i = 0; i < input->count; ++i) {
    const TC_X509_certificate* certificate;
    TC_X509_signature_result signature;
    TC_TLV_result result;
    int valid;
    result = tc_x509_path_certificate(input, i, work, &certificate);
    if (result != TC_TLV_OK)
      return result;
    if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    result = tc_x509_time_window(input->at, input->clock_skew_seconds, &certificate->not_before,
                                 &certificate->not_after, &valid);
    if (result != TC_TLV_OK)
      return result;
    if (!valid) {
      *accepted = 0;
      return TC_TLV_OK;
    }
    signature = TC_X509_issuer_check(certificate, issuer_name, &issuer_key, input->signatures,
                                     input->limits, workspace, work);
    if (signature == TC_X509_SIGNATURE_INVALID) {
      *accepted = 0;
      return TC_TLV_OK;
    }
    result = tc_pki_signature_status(signature);
    if (result != TC_TLV_OK)
      return result;
    if (i + 1 < input->count) {
      const TC_X509_extension_summary* extensions;
      TC_X509_basic_constraints basic;
      result = path_summary_of(input, i, certificate, workspace, work, &extensions);
      if (result != TC_TLV_OK)
        return result;
      basic = extensions->basic;
      /* An intermediate must be a CA with keyCertSign when keyUsage is present. */
      if (!tc_x509_summary_has(extensions, TC_X509_SUMMARY_BASIC_CONSTRAINTS) || !basic.ca ||
          (tc_x509_summary_has(extensions, TC_X509_SUMMARY_KEY_USAGE) &&
           !(extensions->key_usage & TC_KEY_USAGE_CERT_SIGN))) {
        *accepted = 0;
        return TC_TLV_OK;
      }
      if (!extensions->self_issued) {
        if (!remaining) {
          *accepted = 0;
          return TC_TLV_OK;
        }
        --remaining;
        if (input->has_anchor_path_len) {
          if (!anchor_remaining) {
            *accepted = 0;
            return TC_TLV_OK;
          }
          --anchor_remaining;
        }
      }
      if (basic.has_path_length && basic.path_length < remaining)
        remaining = basic.path_length;
    }
    issuer_name = certificate->subject;
    issuer_key = certificate->public_key;
  }
  *accepted = 1;
  return TC_TLV_OK;
}
TC_TLV_result tc_x509_path_constraint_distances(const TC_X509_name_constraints* constraints,
                                                const TC_TLV_limits* limits,
                                                const TC_X509_constraint_workspace* workspace,
                                                size_t* work)
{
  const TC_bytes lists[] = {constraints->permitted, constraints->excluded};
  TC_TLV_limits budget = *limits;
  unsigned i;
  for (i = 0; i < 2; ++i) {
    TC_X509_general_subtrees_reader reader;
    TC_X509_general_subtree subtree;
    TC_TLV_result result;
    if (tc_pki_work_charge(work, lists[i].length) != TC_TLV_OK)
      return TC_TLV_LIMIT;
    result = TC_X509_general_subtrees_init(&reader, lists[i], &budget, workspace->frames);
    if (result != TC_TLV_OK)
      return result;
    while ((result = TC_X509_general_subtree_next(&reader, &subtree)) == TC_TLV_OK) {
      if (tc_pki_work_charge(work, 1) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      if (subtree.minimum || subtree.has_maximum)
        return TC_TLV_UNSUPPORTED;
    }
    if (result != TC_TLV_END)
      return result;
    budget.max_elements -= reader.reader.elements;
  }
  return TC_TLV_OK;
}

/* Apply one constraint set to path entry index after the basic pass. RFC 5280
 * section 6.1.3(b) exempts a self-issued intermediate, which may carry old
 * names through key rollover. */
static TC_TLV_result path_constraint_target(const tc_x509_path_input* input, size_t index,
                                            int charge_step,
                                            const TC_X509_name_constraints* constraints,
                                            const TC_X509_constraint_workspace* workspace,
                                            size_t* work, int* accepted)
{
  const TC_X509_extension_summary* extensions = NULL;
  const TC_X509_certificate* certificate;
  TC_bytes san = {NULL, 0};
  TC_TLV_result result;
  if (charge_step && tc_pki_work_charge(work, 1) != TC_TLV_OK)
    return TC_TLV_LIMIT;
  result = tc_x509_path_certificate(input, index, work, &certificate);
  if (result == TC_TLV_OK)
    result = tc_x509_path_summary(input, index, workspace->names, work, &extensions);
  if (result != TC_TLV_OK)
    return result;
  if (index + 1 < input->count && extensions->self_issued) {
    *accepted = 1;
    return TC_TLV_OK;
  }
  if (tc_x509_summary_has(extensions, TC_X509_SUMMARY_SUBJECT_ALT_NAME))
    san = extensions->values[TC_X509_SUMMARY_SUBJECT_ALT_NAME];
  return tc_x509_certificate_names_check_san(certificate, san, constraints, input->limits,
                                             workspace, work, accepted);
}

TC_TLV_result tc_x509_path_names(const tc_x509_path_input* input,
                                 const TC_X509_constraint_workspace* workspace, size_t* work,
                                 int* accepted)
{
  size_t i, j;
  if (!tc_x509_path_source_valid(input) || !workspace || !workspace->names || !work || !accepted ||
      !input->limits || !input->count)
    return TC_TLV_ARGUMENT;
  if (input->count > input->max_certificates)
    return TC_TLV_LIMIT;
  for (i = 0; i + 1 < input->count; ++i) {
    const TC_X509_extension_summary* extensions;
    TC_X509_name_constraints constraints;
    TC_TLV_result result;
    result = tc_x509_path_summary(input, i, workspace->names, work, &extensions);
    if (result != TC_TLV_OK)
      return result;
    if (!tc_x509_summary_has(extensions, TC_X509_SUMMARY_NAME_CONSTRAINTS))
      continue;
    const TC_bytes value = extensions->values[TC_X509_SUMMARY_NAME_CONSTRAINTS];
    result = TC_X509_name_constraints_read(value, input->limits, &constraints);
    if (result != TC_TLV_OK)
      return result;
    result = tc_x509_path_constraint_distances(&constraints, input->limits, workspace, work);
    if (result != TC_TLV_OK)
      return result;
    for (j = i + 1; j < input->count; ++j) {
      int valid;
      /* Constraint spans borrow the issuer DER throughout this pass. */
      result = path_constraint_target(input, j, 1, &constraints, workspace, work, &valid);
      if (result != TC_TLV_OK)
        return result;
      if (!valid) {
        *accepted = 0;
        return TC_TLV_OK;
      }
    }
  }
  *accepted = 1;
  return TC_TLV_OK;
}

void tc_x509_policy_counters_advance(tc_x509_policy_counters* counters,
                                     const tc_x509_policy_controls* controls, int self_issued,
                                     int target)
{
  const TC_X509_policy_constraints* constraints = &controls->constraints;
  if (target) {
    if (counters->explicit_policy)
      --counters->explicit_policy;
    if (constraints->has_require_explicit_policy && !constraints->require_explicit_policy)
      counters->explicit_policy = 0;
    return;
  }
  if (!self_issued) {
    if (counters->explicit_policy)
      --counters->explicit_policy;
    if (counters->mapping)
      --counters->mapping;
    if (counters->any)
      --counters->any;
  }
  if (constraints->has_require_explicit_policy &&
      constraints->require_explicit_policy < counters->explicit_policy)
    counters->explicit_policy = constraints->require_explicit_policy;
  if (constraints->has_inhibit_policy_mapping &&
      constraints->inhibit_policy_mapping < counters->mapping)
    counters->mapping = constraints->inhibit_policy_mapping;
  if (controls->has_inhibit_any && controls->inhibit_any < counters->any)
    counters->any = controls->inhibit_any;
}
static TC_TLV_result certificate_policies(const TC_X509_extension_summary* extensions, int target,
                                          const TC_TLV_limits* limits,
                                          const tc_x509_policy_workspace* workspace, size_t* work,
                                          size_t* policy_count, size_t* mapping_count,
                                          tc_x509_policy_controls* controls)
{
  TC_TLV_result result;
  *policy_count = *mapping_count = 0;
  tc_x509_policy_controls_from_summary(extensions, controls);
  if (tc_x509_summary_has(extensions, TC_X509_SUMMARY_POLICIES)) {
    const TC_bytes value = extensions->values[TC_X509_SUMMARY_POLICIES];
    const int critical = tc_x509_summary_critical(extensions, TC_X509_SUMMARY_POLICIES);
    TC_X509_policy_reader policies;
    TC_X509_policy policy;
    result = TC_X509_policies_init(&policies, value, limits, workspace->policies,
                                   workspace->policy_capacity);
    if (result != TC_TLV_OK)
      return result;
    for (;;) {
      /* Includes the decoder's comparisons against previously seen OIDs. */
      if (tc_pki_work_charge(work, value.length) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      result = TC_X509_policy_next(&policies, &policy);
      if (result != TC_TLV_OK)
        break;
      result = tc_x509_policy_qualifiers_check(&policy, critical, limits, workspace->frames, work);
      if (result != TC_TLV_OK)
        return result;
    }
    if (result != TC_TLV_END)
      return result;
    *policy_count = policies.count;
  }
  if (tc_x509_summary_has(extensions, TC_X509_SUMMARY_POLICY_MAPPINGS)) {
    const TC_bytes value = extensions->values[TC_X509_SUMMARY_POLICY_MAPPINGS];
    TC_TLV_reader mappings;
    TC_X509_policy_mapping mapping;
    if (target && tc_x509_summary_critical(extensions, TC_X509_SUMMARY_POLICY_MAPPINGS))
      return TC_TLV_INVALID;
    result = TC_X509_policy_mappings_init(&mappings, value, limits);
    if (result != TC_TLV_OK)
      return result;
    while ((result = TC_X509_policy_mapping_next(&mappings, &mapping)) == TC_TLV_OK) {
      if (tc_pki_work_charge(work, value.length) != TC_TLV_OK)
        return TC_TLV_LIMIT;
      if (*mapping_count == workspace->mapping_capacity)
        return TC_TLV_LIMIT;
      workspace->mappings[(*mapping_count)++] = mapping;
    }
    if (result != TC_TLV_END)
      return result;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_path_policies(const tc_x509_path_input* input,
                                    const tc_x509_policy_options* options,
                                    TC_bytes anchor_policy_set,
                                    const tc_x509_policy_workspace* workspace, size_t* work,
                                    size_t* count, int* accepted)
{
  tc_x509_policy_counters counters;
  TC_TLV_result result;
  size_t i, output_count;
  if (!tc_x509_path_source_valid(input) || !options || !workspace || !workspace->graph ||
      !workspace->names || !work || !count || !accepted || !input->limits || !input->count ||
      (workspace->policy_capacity && !workspace->policies) ||
      (workspace->mapping_capacity && !workspace->mappings) ||
      (workspace->output_capacity && !workspace->output) ||
      (options->initial_count && !options->initial))
    return TC_TLV_ARGUMENT;
  if (input->count > input->max_certificates || input->count == SIZE_MAX)
    return TC_TLV_LIMIT;
  counters.explicit_policy = options->require_explicit ? 0 : input->count + 1;
  counters.mapping = options->inhibit_mapping ? 0 : input->count + 1;
  counters.any = options->inhibit_any ? 0 : input->count + 1;
  result = tc_x509_policy_graph_init(workspace->graph);
  if (result != TC_TLV_OK)
    return result;
  for (i = 0; i < input->count; ++i) {
    const TC_X509_extension_summary* extensions;
    tc_x509_policy_controls controls;
    size_t policy_count, mapping_count;
    const int target = i + 1 == input->count;
    int self_issued;
    result = tc_x509_path_summary(input, i, workspace->names, work, &extensions);
    if (result != TC_TLV_OK)
      return result;
    self_issued = !target && extensions->self_issued;
    result = certificate_policies(extensions, target, input->limits, workspace, work, &policy_count,
                                  &mapping_count, &controls);
    if (result != TC_TLV_OK)
      return result;
    result = tc_x509_policy_graph_step(workspace->graph, workspace->policies, policy_count,
                                       counters.any > 0 || (self_issued && !target), work);
    if (result != TC_TLV_OK)
      return result;
    if (!counters.explicit_policy && !workspace->graph->nodes[0].alive) {
      *accepted = 0;
      *count = 0;
      return TC_TLV_OK;
    }
    if (!target) {
      result = tc_x509_policy_graph_map(workspace->graph, workspace->mappings, mapping_count,
                                        counters.mapping > 0, work);
      if (result != TC_TLV_OK)
        return result;
    }
    tc_x509_policy_counters_advance(&counters, &controls, self_issued, target);
  }
  const tc_x509_policy_filter filter = {options->initial, options->initial_count,
                                        anchor_policy_set};
  result = tc_x509_policy_graph_output(workspace->graph, &filter, input->limits, workspace->output,
                                       workspace->output_capacity, work, &output_count);
  if (result != TC_TLV_OK)
    return result;
  *count = output_count;
  *accepted = counters.explicit_policy > 0 || output_count > 0;
  return TC_TLV_OK;
}
void tc_x509_path_storage_plan(tc_pki_storage_plan* plan, const TC_X509_path_workspace* workspace)
{
  TC_PKI_PLAN_WRITE(plan, workspace->frames.data, workspace->frames.capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->oids, workspace->oid_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->names.left, workspace->names.scalar_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->names.right, workspace->names.scalar_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->names.matched, workspace->names.attribute_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->nodes, workspace->node_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->edges, workspace->edge_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->expected, workspace->expected_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->mappings, workspace->mapping_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->policies, workspace->policy_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->certificates, workspace->certificate_capacity);
  TC_PKI_PLAN_WRITE(plan, workspace->summaries, workspace->summary_capacity);
}

void tc_x509_policy_plan_inputs(tc_pki_storage_plan* plan, const TC_bytes* initial_policies,
                                size_t initial_policy_count, TC_bytes purpose,
                                const TC_X509_name_constraints* anchor_names)
{
  const TC_bytes fields[] = {purpose, anchor_names->permitted, anchor_names->excluded};
  TC_PKI_PLAN_INPUT(plan, initial_policies, initial_policy_count);
  tc_pki_storage_plan_input_spans(plan, fields, sizeof fields / sizeof *fields);
  tc_pki_storage_plan_input_spans(plan, initial_policies, initial_policy_count);
}

void tc_x509_path_options_plan_inputs(tc_pki_storage_plan* plan,
                                      const TC_X509_path_options* options)
{
  tc_x509_policy_plan_inputs(plan, options->initial_policies, options->initial_policy_count,
                             options->purpose, &options->anchor_names);
}

/* Validation consumes the caller's work directly: storage checks are part of
 * the operation's cost, and work is one of the checked writes. */
static TC_TLV_result path_storage(const TC_bytes* chain, size_t count,
                                  const TC_X509_store_anchor* anchor,
                                  const TC_X509_path_options* options,
                                  const TC_X509_path_workspace* workspace, TC_X509_path_report* out,
                                  size_t* work)
{
  TC_bytes writes[TC_X509_PATH_STORAGE_COUNT + 1];
  const TC_bytes anchor_fields[] = {anchor->trust.name,
                                    anchor->trust.public_key.algorithm.oid,
                                    anchor->trust.public_key.algorithm.parameters,
                                    anchor->trust.public_key.key,
                                    anchor->trust.public_key.modulus,
                                    anchor->trust.public_key.exponent,
                                    anchor->trust.public_key.curve_oid,
                                    anchor->names.permitted,
                                    anchor->names.excluded,
                                    anchor->key_id,
                                    anchor->policy_set,
                                    anchor->extensions,
                                    anchor->certificate_extensions};
  tc_pki_storage_plan plan;
  TC_TLV_result result;

  tc_pki_storage_plan_begin(&plan, writes, sizeof writes / sizeof *writes, *work);
  tc_x509_path_storage_plan(&plan, workspace);
  TC_PKI_PLAN_WRITE(&plan, out, 1);
  tc_pki_storage_plan_seal(&plan);
  TC_PKI_PLAN_INPUT(&plan, chain, count);
  TC_PKI_PLAN_INPUT(&plan, anchor, 1);
  TC_PKI_PLAN_INPUT(&plan, options, 1);
  TC_PKI_PLAN_INPUT(&plan, workspace, 1);
  tc_pki_storage_plan_input_spans(&plan, anchor_fields,
                                  sizeof anchor_fields / sizeof *anchor_fields);
  tc_pki_storage_plan_input_spans(&plan, chain, count);
  tc_x509_path_options_plan_inputs(&plan, options);
  result = tc_pki_storage_plan_finish(&plan, NULL);
  *work = plan.budget;
  return result;
}

/* CertPathControls field that replaces one certificate path control
 * (RFC 5914 section 2.5), or 0 for other extensions. */
static unsigned anchor_replacement(unsigned id)
{
  switch (id) {
  case TC_PKI_EXT_CERTIFICATE_POLICIES:
    return TC_X509_ANCHOR_REPLACED_POLICY_SET;
  case TC_PKI_EXT_POLICY_CONSTRAINTS:
  case TC_PKI_EXT_INHIBIT_ANY_POLICY:
    return TC_X509_ANCHOR_REPLACED_POLICY_FLAGS;
  case TC_PKI_EXT_NAME_CONSTRAINTS:
    return TC_X509_ANCHOR_REPLACED_NAMES;
  case TC_PKI_EXT_BASIC_CONSTRAINTS:
    return TC_X509_ANCHOR_REPLACED_PATH_LEN;
  default:
    return 0;
  }
}

/* Report whether the anchor record reflects one path-control extension.
 * RFC 5914 section 2.5 enforces an anchor certificate's path controls unless
 * CertPathControls replaces them. Validation reads only the record fields,
 * so a control those fields miss would be dropped. A certificate control
 * marked in replaced_controls is superseded by its field. trust_anchor_info
 * selects exts, where only a basicConstraints pathLen can occur and it may
 * only lower path_len. */
static TC_TLV_result anchor_control_applied(const TC_X509_store_anchor* anchor, unsigned id,
                                            const TC_X509_extension* extension,
                                            int trust_anchor_info, const TC_TLV_limits* limits,
                                            int* applied)
{
  const TC_bytes value = extension->value;
  TC_TLV_result result;
  *applied = 1;
  if (!trust_anchor_info && (anchor->replaced_controls & anchor_replacement(id)))
    return TC_TLV_OK;
  switch (id) {
  case TC_PKI_EXT_NAME_CONSTRAINTS:
    *applied = anchor->names.permitted.length || anchor->names.excluded.length;
    return TC_TLV_OK;
  case TC_PKI_EXT_CERTIFICATE_POLICIES:
    *applied = anchor->policy_set.data != NULL;
    return TC_TLV_OK;
  case TC_PKI_EXT_POLICY_CONSTRAINTS: {
    TC_X509_policy_constraints constraints;
    result = TC_X509_policy_constraints_read(value, limits, &constraints);
    if (result != TC_TLV_OK)
      return result;
    if ((constraints.has_require_explicit_policy &&
         !(anchor->policy_flags & TC_X509_PATH_REQUIRE_EXPLICIT_POLICY)) ||
        (constraints.has_inhibit_policy_mapping &&
         !(anchor->policy_flags & TC_X509_PATH_INHIBIT_MAPPING)))
      *applied = 0;
    return TC_TLV_OK;
  }
  case TC_PKI_EXT_INHIBIT_ANY_POLICY:
    *applied = (anchor->policy_flags & TC_X509_PATH_INHIBIT_ANY_POLICY) != 0;
    return TC_TLV_OK;
  case TC_PKI_EXT_BASIC_CONSTRAINTS: {
    TC_X509_basic_constraints basic;
    result = TC_X509_basic_constraints_read(value, limits, &basic);
    if (result != TC_TLV_OK)
      return result;
    if (basic.has_path_length &&
        (!anchor->has_path_len || (trust_anchor_info && anchor->path_len > basic.path_length)))
      *applied = 0;
    return TC_TLV_OK;
  }
  default:
    return TC_TLV_OK;
  }
}

/* Check one of the anchor's extension lists before path processing.
 * TrustAnchorInfo exts must not carry certificatePolicies,
 * policyConstraints, inhibitAnyPolicy or nameConstraints (RFC 5914 section
 * 2.6), so any of them there is INVALID. A path control that the record
 * fields do not reflect is UNSUPPORTED, and so is any other critical
 * extension this validator does not implement. */
static TC_TLV_result anchor_extensions_check(const TC_X509_store_anchor* anchor, TC_bytes contents,
                                             int trust_anchor_info, const TC_TLV_limits* limits,
                                             size_t* work)
{
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result;
  if (!contents.data)
    return contents.length ? TC_TLV_ARGUMENT : TC_TLV_OK;
  result = TC_TLV_reader_init(&reader, contents, TC_TLV_DER, limits);
  if (result != TC_TLV_OK)
    return result;
  while ((result = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
    const unsigned id = tc_pki_extension_id(&extension);
    const int path_control = tc_pki_extension_path_control(id);
    int applied;
    if (trust_anchor_info && path_control)
      return TC_TLV_INVALID;
    if (extension.critical && id != TC_PKI_EXT_SUBJECT_KEY_IDENTIFIER &&
        id != TC_PKI_EXT_KEY_USAGE && id != TC_PKI_EXT_BASIC_CONSTRAINTS && !path_control)
      return TC_TLV_UNSUPPORTED;
    result = anchor_control_applied(anchor, id, &extension, trust_anchor_info, limits, &applied);
    if (result != TC_TLV_OK)
      return result;
    if (!applied)
      return TC_TLV_UNSUPPORTED;
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

/* Caller options checked at the entry, before any work is charged: a valid
 * validation time, and initial policies and a purpose with well-formed OID
 * contents (X.690 section 8.19). bytes receives the OID bytes examined, which
 * validation charges once the storage checks pass. The caller has bounded
 * initial_policy_count by the parsing element limit. */
static int path_options_valid(const TC_X509_path_options* options, size_t* bytes)
{
  size_t i, examined = 0;
  if (TC_X509_time_check(&options->at) != TC_TLV_OK ||
      (options->initial_policy_count && !options->initial_policies))
    return 0;
  for (i = 0; i < options->initial_policy_count; ++i) {
    const TC_bytes oid = options->initial_policies[i];
    if (TC_DER_oid_contents(oid) != TC_TLV_OK || oid.length > SIZE_MAX - examined)
      return 0;
    examined += oid.length;
  }
  if (options->purpose.length && (TC_DER_oid_contents(options->purpose) != TC_TLV_OK ||
                                  options->purpose.length > SIZE_MAX - examined))
    return 0;
  *bytes = examined + options->purpose.length;
  return 1;
}

/* One validation of an ordered chain against a stored anchor (RFC 5280
 * section 6.1). The entry records the caller's inputs and the phases share
 * the workspace views, the caller's work counter and the policy result. */
typedef struct {
  const TC_bytes* chain;
  size_t count;
  const TC_X509_store_anchor* anchor;
  const TC_X509_path_options* options;
  const TC_X509_path_workspace* workspace;
  /* Remaining work units, charged by the phases. */
  size_t* work;
  /* Views of the workspace arrays. input reads the chain through parser into
   * workspace->certificates until the basic phase fills that cache. */
  TC_X509_workspace parser;
  TC_X509_constraint_workspace names;
  tc_x509_path_input input;
  /* Valid policy count in workspace->policies after the policy phase. */
  size_t policy_count;
} path_validation;

/* Status of a pass that reports a result and an acceptance verdict. VALID
 * lets the next phase run. */
static TC_X509_path_status path_pass_status(TC_TLV_result result, int accepted)
{
  if (result != TC_TLV_OK)
    return tc_x509_path_status(result);
  return accepted ? TC_X509_PATH_VALID : TC_X509_PATH_INVALID;
}

/* Validate the caller's arguments once, before any work is charged.
 * option_bytes receives the option OID bytes examined. */
static TC_X509_path_status
path_arguments_check(const path_validation* v, const TC_X509_path_report* out, size_t* option_bytes)
{
  const TC_X509_store_anchor* anchor = v->anchor;
  const TC_X509_path_options* options = v->options;
  const TC_X509_path_workspace* workspace = v->workspace;
  if (!v->chain || !anchor || !options || !workspace || !v->work || !out ||
      (options->flags & ~(unsigned)TC_X509_PATH_SUPPORTED_FLAGS))
    return TC_X509_PATH_ERROR;
  if (anchor->x509_unusable)
    return TC_X509_PATH_INVALID;
  if ((anchor->policy_flags &
       ~(unsigned)(TC_X509_PATH_REQUIRE_EXPLICIT_POLICY | TC_X509_PATH_INHIBIT_MAPPING |
                   TC_X509_PATH_INHIBIT_ANY_POLICY)) ||
      (anchor->replaced_controls &
       ~(unsigned)(TC_X509_ANCHOR_REPLACED_POLICY_SET | TC_X509_ANCHOR_REPLACED_POLICY_FLAGS |
                   TC_X509_ANCHOR_REPLACED_NAMES | TC_X509_ANCHOR_REPLACED_PATH_LEN)))
    return TC_X509_PATH_ERROR;
  if (!v->count)
    return TC_X509_PATH_INVALID;
  if (v->count > options->max_certificates || v->count > SIZE_MAX / sizeof *v->chain ||
      options->initial_policy_count > options->parsing.max_elements)
    return TC_X509_PATH_LIMIT;
  if (!workspace->certificates || v->count > workspace->certificate_capacity ||
      !workspace->summaries || v->count > workspace->summary_capacity)
    return TC_X509_PATH_LIMIT;
  if (!path_options_valid(options, option_bytes))
    return TC_X509_PATH_ERROR;
  return TC_X509_PATH_VALID;
}

/* Check the stored anchor's extension lists, which supply the trust anchor
 * information of RFC 5280 section 6.1.1(d). TrustAnchorInfo exts come first,
 * then the anchor certificate's extensions. */
static TC_X509_path_status path_anchor_check(const path_validation* v)
{
  const TC_TLV_limits* limits = &v->options->parsing;
  TC_TLV_result result =
      anchor_extensions_check(v->anchor, v->anchor->extensions, 1, limits, v->work);
  if (result == TC_TLV_OK)
    result =
        anchor_extensions_check(v->anchor, v->anchor->certificate_extensions, 0, limits, v->work);
  return tc_x509_path_status(result);
}

/* Charge one unit per chain entry and bound the total DER bytes by
 * max_input. Then charge the option OID bytes the entry examined. */
static TC_X509_path_status path_chain_check(const path_validation* v, size_t option_bytes)
{
  size_t i, total = 0;
  for (i = 0; i < v->count; ++i) {
    if (tc_pki_work_charge(v->work, 1) != TC_TLV_OK)
      return TC_X509_PATH_LIMIT;
    if (!v->chain[i].length)
      return TC_X509_PATH_INVALID;
    if (v->chain[i].length > v->options->max_input - total)
      return TC_X509_PATH_LIMIT;
    total += v->chain[i].length;
  }
  if (tc_pki_work_charge(v->work, option_bytes) != TC_TLV_OK)
    return TC_X509_PATH_LIMIT;
  return TC_X509_PATH_VALID;
}

/* Build the parser, name-constraint and pass-input views over the workspace
 * and clear the summary cache that the passes fill afresh. Charges no work. */
static void path_views_init(path_validation* v)
{
  const TC_X509_path_workspace* workspace = v->workspace;
  const TC_X509_path_options* options = v->options;
  tc_x509_path_input* input = &v->input;
  size_t i;
  v->parser.frames = workspace->frames;
  v->parser.extension_oids = workspace->oids;
  v->parser.extension_capacity = workspace->oid_capacity;
  v->names.frames = workspace->frames;
  v->names.names = &workspace->names;
  memset(input, 0, sizeof *input);
  input->count = v->count;
  input->max_certificates = options->max_certificates;
  input->max_input = options->max_input;
  input->anchor = &v->anchor->trust;
  input->at = &options->at;
  input->clock_skew_seconds = options->clock_skew_seconds;
  input->signatures = &options->signatures;
  input->anchor_path_len = v->anchor->path_len;
  input->has_anchor_path_len = v->anchor->has_path_len;
  input->limits = &options->parsing;
  input->encoded = v->chain;
  input->parser = &v->parser;
  input->cache = workspace->certificates;
  for (i = 0; i < v->count; ++i)
    workspace->summaries[i].ready = 0;
  input->summaries = workspace->summaries;
}

/* Basic certificate processing: signature, validity and issuer name
 * (RFC 5280 section 6.1.3(a)), and CA status, keyCertSign and path length
 * for intermediates (6.1.4(k) to (n)). The pass parses every certificate into
 * the cache, so later phases read the cached views. */
static TC_X509_path_status path_basic_phase(path_validation* v)
{
  int accepted = 0;
  const TC_TLV_result result =
      tc_x509_path_basic(&v->input, &v->workspace->names, v->work, &accepted);
  const TC_X509_path_status status = path_pass_status(result, accepted);
  if (status == TC_X509_PATH_VALID)
    v->input.certificates = v->workspace->certificates;
  return status;
}

/* Apply one initial subtree set to every path certificate (RFC 5280 section
 * 6.1.1(b), (c)). An empty set applies no constraint. */
static TC_X509_path_status path_initial_subtrees(const path_validation* v,
                                                 const TC_X509_name_constraints* constraints)
{
  size_t i;
  TC_TLV_result result;
  if (!constraints->permitted.length && !constraints->excluded.length)
    return TC_X509_PATH_VALID;
  result = tc_x509_path_constraint_distances(constraints, v->input.limits, &v->names, v->work);
  if (result != TC_TLV_OK)
    return tc_x509_path_status(result);
  for (i = 0; i < v->count; ++i) {
    int accepted = 0;
    /* Sequence the pass before reading accepted: argument order is unspecified. */
    const TC_TLV_result pass =
        path_constraint_target(&v->input, i, 0, constraints, &v->names, v->work, &accepted);
    const TC_X509_path_status status = path_pass_status(pass, accepted);
    if (status != TC_X509_PATH_VALID)
      return status;
  }
  return TC_X509_PATH_VALID;
}

/* Name constraints (RFC 5280 section 6.1.3(b), (c) and 6.1.4(g)): each
 * intermediate's nameConstraints, then the application's and the stored
 * anchor's subtrees, each applied independently. */
static TC_X509_path_status path_names_phase(const path_validation* v)
{
  int accepted = 0;
  const TC_TLV_result result = tc_x509_path_names(&v->input, &v->names, v->work, &accepted);
  TC_X509_path_status status = path_pass_status(result, accepted);
  if (status == TC_X509_PATH_VALID)
    status = path_initial_subtrees(v, &v->options->anchor_names);
  if (status == TC_X509_PATH_VALID)
    status = path_initial_subtrees(v, &v->anchor->names);
  return status;
}

/* Certificate policies (RFC 5280 sections 6.1.1(e) to (i), 6.1.3(d) to (f),
 * 6.1.4(a), (b), (h) to (j) and 6.1.5(a), (b), (g)). The caller's flags and
 * the anchor's policy flags both set the initial inhibit and explicit-policy
 * inputs. The valid policy set lands in workspace->policies. */
static TC_X509_path_status path_policy_phase(path_validation* v)
{
  const TC_X509_path_workspace* workspace = v->workspace;
  const TC_X509_path_options* options = v->options;
  const unsigned flags = options->flags | v->anchor->policy_flags;
  tc_x509_policy_graph graph;
  tc_x509_policy_options policy_options;
  tc_x509_policy_workspace policy_workspace;
  TC_TLV_result result;
  int accepted = 0;
  memset(&graph, 0, sizeof graph);
  graph.nodes = workspace->nodes;
  graph.node_capacity = workspace->node_capacity;
  graph.edges = workspace->edges;
  graph.edge_capacity = workspace->edge_capacity;
  graph.expected = workspace->expected;
  graph.expected_capacity = workspace->expected_capacity;
  policy_options.initial = options->initial_policies;
  policy_options.initial_count = options->initial_policy_count;
  policy_options.require_explicit = (flags & TC_X509_PATH_REQUIRE_EXPLICIT_POLICY) != 0;
  policy_options.inhibit_mapping = (flags & TC_X509_PATH_INHIBIT_MAPPING) != 0;
  policy_options.inhibit_any = (flags & TC_X509_PATH_INHIBIT_ANY_POLICY) != 0;
  policy_workspace.graph = &graph;
  policy_workspace.policies = workspace->oids;
  policy_workspace.policy_capacity = workspace->oid_capacity;
  policy_workspace.mappings = workspace->mappings;
  policy_workspace.mapping_capacity = workspace->mapping_capacity;
  policy_workspace.output = workspace->policies;
  policy_workspace.output_capacity = workspace->policy_capacity;
  policy_workspace.names = &workspace->names;
  policy_workspace.frames = workspace->frames;
  result = tc_x509_path_policies(&v->input, &policy_options, v->anchor->policy_set,
                                 &policy_workspace, v->work, &v->policy_count, &accepted);
  return path_pass_status(result, accepted);
}

/* Target usage and the remaining critical extensions: keyUsage and
 * extendedKeyUsage against the caller's purpose (RFC 5280 sections 4.2.1.3
 * and 4.2.1.12), and unrecognized critical extensions (6.1.4(o), 6.1.5(f)). */
static TC_X509_path_status path_usage_phase(const path_validation* v)
{
  const TC_X509_path_options* options = v->options;
  tc_x509_path_usage usage;
  tc_x509_extension_workspace extension_workspace;
  int accepted = 0;
  usage.purpose = options->purpose;
  usage.key_usage = options->key_usage;
  usage.require_key_usage = (options->flags & TC_X509_PATH_REQUIRE_KEY_USAGE) != 0;
  usage.require_extended_key_usage =
      (options->flags & TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE) != 0;
  usage.inhibit_any_purpose = (options->flags & TC_X509_PATH_INHIBIT_ANY_PURPOSE) != 0;
  extension_workspace.oids = v->workspace->oids;
  extension_workspace.oid_capacity = v->workspace->oid_capacity;
  extension_workspace.names = v->names;
  const TC_TLV_result result =
      tc_x509_path_extensions(&v->input, &usage, &extension_workspace, v->work, &accepted);
  return path_pass_status(result, accepted);
}

/* Write the outputs of RFC 5280 section 6.1.6: the target's public key and
 * the valid policy set. work_used counts the units spent since initial_work. */
static TC_X509_path_status path_result_write(const path_validation* v, size_t initial_work,
                                             TC_X509_path_report* out)
{
  const TC_X509_certificate* target = NULL;
  TC_X509_path_report validated;
  const TC_TLV_result result = tc_x509_path_certificate(&v->input, v->count - 1, v->work, &target);
  if (result != TC_TLV_OK)
    return tc_x509_path_status(result);
  /* Assemble the result before the single write to out. */
  validated.public_key = target->public_key;
  validated.policies = v->workspace->policies;
  validated.policy_count = v->policy_count;
  validated.work_used = initial_work - *v->work;
  *out = validated;
  return TC_X509_PATH_VALID;
}

/* Run the validation phases in order. Each phase returns VALID to continue,
 * and the first other status ends validation with out unchanged. */
TC_X509_path_status tc_x509_path_validate_anchor(const TC_bytes* chain, size_t count,
                                                 const TC_X509_store_anchor* anchor,
                                                 const TC_X509_path_options* options,
                                                 const TC_X509_path_workspace* workspace,
                                                 size_t* work, TC_X509_path_report* out)
{
  path_validation v;
  size_t initial_work, option_bytes = 0;
  TC_X509_path_status status;
  v.chain = chain;
  v.count = count;
  v.anchor = anchor;
  v.options = options;
  v.workspace = workspace;
  v.work = work;
  v.policy_count = 0;
  status = path_arguments_check(&v, out, &option_bytes);
  if (status != TC_X509_PATH_VALID)
    return status;
  initial_work = *work;
  status = tc_x509_path_status(path_storage(chain, count, anchor, options, workspace, out, work));
  if (status == TC_X509_PATH_VALID)
    status = path_anchor_check(&v);
  if (status == TC_X509_PATH_VALID)
    status = path_chain_check(&v, option_bytes);
  if (status != TC_X509_PATH_VALID)
    return status;
  path_views_init(&v);
  status = path_basic_phase(&v);
  if (status == TC_X509_PATH_VALID)
    status = path_names_phase(&v);
  if (status == TC_X509_PATH_VALID)
    status = path_policy_phase(&v);
  if (status == TC_X509_PATH_VALID)
    status = path_usage_phase(&v);
  if (status == TC_X509_PATH_VALID)
    status = path_result_write(&v, initial_work, out);
  return status;
}

TC_X509_path_status tc_x509_path_validate_budget(const TC_bytes* chain, size_t count,
                                                 const TC_X509_trust_anchor* anchor,
                                                 const TC_X509_path_options* options,
                                                 const TC_X509_path_workspace* workspace,
                                                 size_t* work, TC_X509_path_report* out)
{
  TC_X509_store_anchor stored = {0};
  if (!anchor)
    return TC_X509_PATH_ERROR;
  stored.trust = *anchor;
  return tc_x509_path_validate_anchor(chain, count, &stored, options, workspace, work, out);
}

TC_X509_path_status TC_X509_path_validate_with_anchor(const TC_bytes* chain, size_t count,
                                                      const TC_X509_store_anchor* anchor,
                                                      const TC_X509_path_options* options,
                                                      const TC_X509_path_workspace* workspace,
                                                      TC_X509_path_report* out)
{
  size_t work;
  if (!options)
    return TC_X509_PATH_ERROR;
  work = options->max_work;
  return tc_x509_path_validate_anchor(chain, count, anchor, options, workspace, &work, out);
}

TC_X509_path_status TC_X509_path_validate(const TC_bytes* chain, size_t count,
                                          const TC_X509_trust_anchor* anchor,
                                          const TC_X509_path_options* options,
                                          const TC_X509_path_workspace* workspace,
                                          TC_X509_path_report* out)
{
  size_t work;
  if (!options)
    return TC_X509_PATH_ERROR;
  work = options->max_work;
  return tc_x509_path_validate_budget(chain, count, anchor, options, workspace, &work, out);
}
#endif
