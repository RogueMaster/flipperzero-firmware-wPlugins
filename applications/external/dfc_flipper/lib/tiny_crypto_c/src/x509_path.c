/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/x509.h>
#if TC_ENABLE_X509_PATH
#include "x509_path_internal.h"
#include "pki_status_internal.h"
#include "pki_internal.h"
#include "pki_storage_internal.h"
#include "pki_extensions_internal.h"
#include "internal.h"

static int path_source_valid(const tc_x509_path_input* input)
{
  return input && (input->certificates || (input->encoded && input->parser));
}

static TC_bytes path_encoded(const tc_x509_path_input* input, size_t index)
{
  return input->certificates ? input->certificates[index].encoded : input->encoded[index];
}

static TC_TLV_result path_certificate(const tc_x509_path_input* input, size_t index,
    TC_X509_certificate* scratch, size_t* work, const TC_X509_certificate** certificate)
{
  TC_bytes encoded;
  TC_TLV_result result;
  if (input->certificates) {
    *certificate = &input->certificates[index];
    return TC_TLV_OK;
  }
  encoded = input->encoded[index];
  if (tc_x509_path_charge(work, encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
  result = TC_X509_read(encoded.data, encoded.length, input->limits, input->parser, scratch);
  if (result != TC_TLV_OK) return result;
  *certificate = scratch;
  return TC_TLV_OK;
}

static TC_TLV_result ca_extensions(const TC_X509_certificate* certificate,
    const TC_TLV_limits* limits, size_t* work, TC_X509_basic_constraints* basic, int* authorized)
{
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result;
  uint16_t usage = 0;
  int has_basic = 0, has_usage = 0;
  memset(basic, 0, sizeof(*basic));
  result = tc_pki_extensions_init(&reader, certificate, limits, work);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
    unsigned id = tc_pki_extension_id(&extension);
    if (id == 19) {
      if (has_basic) return TC_TLV_INVALID;
      has_basic = 1;
      result = TC_X509_basic_constraints_read(extension.value.data, extension.value.length, basic);
      if (result != TC_TLV_OK) return result;
    } else if (id == 15) {
      result = tc_pki_key_usage_value(extension.value,&has_usage,&usage);
      if (result != TC_TLV_OK) return result;
    }
  }
  if (result != TC_TLV_END) return result;
  *authorized = has_basic && basic->ca && (!has_usage || (usage & TC_KEY_USAGE_CERT_SIGN));
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_path_basic(const tc_x509_path_input* input,
    const TC_X509_name_workspace* workspace, size_t* work, int* accepted)
{
  size_t i, j, total = 0, remaining;
  TC_bytes issuer_name;
  TC_X509_public_key issuer_key;
  if (!path_source_valid(input) || !workspace || !work || !accepted || !input->anchor
      || !input->at || !input->limits || !input->count) return TC_TLV_ARGUMENT;
  if (input->count > input->max_certificates) return TC_TLV_LIMIT;
  for (i = 0; i < input->count; ++i) {
    const TC_bytes encoded = path_encoded(input, i);
    if (!encoded.data || !encoded.length) return TC_TLV_ARGUMENT;
    if (encoded.length > input->max_input - total) return TC_TLV_LIMIT;
    total += encoded.length;
    if (tc_x509_path_charge(work, encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
    for (j = 0; j < i; ++j) {
      const TC_bytes previous = path_encoded(input, j);
      if (tc_x509_path_charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
      if (encoded.length == previous.length) {
        if (tc_x509_path_charge(work, encoded.length) != TC_TLV_OK) return TC_TLV_LIMIT;
        if (tc_pki_equal(encoded, previous)) { *accepted = 0; return TC_TLV_OK; }
      }
    }
  }
  remaining = input->count - 1;
  issuer_name = input->anchor->name; issuer_key = input->anchor->public_key;
  for (i = 0; i < input->count; ++i) {
    TC_X509_certificate scratch;
    const TC_X509_certificate* certificate;
    TC_X509_signature_result signature;
    TC_TLV_result result;
    int valid;
    result = path_certificate(input, i, &scratch, work, &certificate);
    if (result != TC_TLV_OK) return result;
    if (tc_x509_path_charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = TC_X509_valid_at(certificate, input->at, &valid);
    if (result != TC_TLV_OK) return result;
    if (!valid) { *accepted = 0; return TC_TLV_OK; }
    signature = TC_X509_issuer_check(certificate, issuer_name, &issuer_key, input->signatures,
      input->limits, workspace, work);
    if (signature == TC_X509_SIGNATURE_INVALID) { *accepted = 0; return TC_TLV_OK; }
    result = tc_pki_signature_status(signature);
    if (result != TC_TLV_OK) return result;
    if (i + 1 < input->count) {
      TC_X509_basic_constraints basic;
      int self_issued;
      result = ca_extensions(certificate, input->limits, work, &basic, &valid);
      if (result != TC_TLV_OK) return result;
      if (!valid) { *accepted = 0; return TC_TLV_OK; }
      result = TC_X509_name_equal(certificate->subject, certificate->issuer,
        input->limits, workspace, work, &self_issued);
      if (result != TC_TLV_OK) return result;
      if (!self_issued) {
        if (!remaining) { *accepted = 0; return TC_TLV_OK; }
        --remaining;
      }
      if (basic.has_path_length && basic.path_length < remaining) remaining = basic.path_length;
    }
    issuer_name = certificate->subject; issuer_key = certificate->public_key;
  }
  *accepted = 1;
  return TC_TLV_OK;
}
static TC_TLV_result constraint_distances(const TC_X509_name_constraints* constraints,
    const TC_TLV_limits* limits, const TC_X509_constraint_workspace* workspace, size_t* work)
{
  const TC_bytes lists[] = {constraints->permitted, constraints->excluded};
  TC_TLV_limits budget = *limits;
  unsigned i;
  for (i = 0; i < 2; ++i) {
    TC_TLV_reader reader;
    TC_X509_general_subtree subtree;
    TC_TLV_result result;
    if (tc_x509_path_charge(work, lists[i].length) != TC_TLV_OK) return TC_TLV_LIMIT;
    result = TC_TLV_reader_init(&reader, lists[i].data, lists[i].length, TC_TLV_DER, &budget);
    if (result != TC_TLV_OK) return result;
    while ((result = TC_X509_general_subtree_next(&reader, workspace->frames,
        workspace->frame_capacity, &subtree)) == TC_TLV_OK) {
      if (tc_x509_path_charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
      if (subtree.minimum || subtree.has_maximum) return TC_TLV_UNSUPPORTED;
    }
    if (result != TC_TLV_END) return result;
    budget.max_elements -= reader.elements;
  }
  return TC_TLV_OK;
}

TC_TLV_result tc_x509_path_names(const tc_x509_path_input* input,
    const TC_X509_constraint_workspace* workspace, size_t* work, int* accepted)
{
  size_t i, j;
  if (!path_source_valid(input) || !workspace || !workspace->names || !work || !accepted
      || !input->limits || !input->count) return TC_TLV_ARGUMENT;
  if (input->count > input->max_certificates) return TC_TLV_LIMIT;
  for (i = 0; i + 1 < input->count; ++i) {
    TC_X509_certificate scratch;
    const TC_X509_certificate* issuer;
    TC_TLV_reader reader;
    TC_X509_extension extension;
    TC_X509_name_constraints constraints;
    TC_TLV_result result;
    int found = 0;
    result = path_certificate(input, i, &scratch, work, &issuer);
    if (result != TC_TLV_OK) return result;
    result = tc_pki_extensions_init(&reader, issuer, input->limits, work);
    if (result != TC_TLV_OK) return result;
    while ((result = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
      if (tc_pki_extension_id(&extension) != 30) continue;
      if (found) return TC_TLV_INVALID;
      found = 1;
      result = TC_X509_name_constraints_read(extension.value.data, extension.value.length, input->limits, &constraints);
      if (result != TC_TLV_OK) return result;
    }
    if (result != TC_TLV_END) return result;
    if (!found) continue;
    result = constraint_distances(&constraints, input->limits, workspace, work);
    if (result != TC_TLV_OK) return result;
    for (j = i + 1; j < input->count; ++j) {
      const TC_X509_certificate* certificate;
      int valid;
      /* Constraint spans still refer to issuer DER after scratch is reused. */
      result = path_certificate(input, j, &scratch, work, &certificate);
      if (result != TC_TLV_OK) return result;
      if (tc_x509_path_charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
      /* Key rollover may retain the issuer name. The target never gets this exception. */
      if (j + 1 < input->count) {
        result = TC_X509_name_equal(certificate->subject, certificate->issuer,
          input->limits, workspace->names, work, &valid);
        if (result != TC_TLV_OK) return result;
        if (valid) continue;
      }
      result = TC_X509_certificate_names_check(certificate, &constraints,
        input->limits, workspace, work, &valid);
      if (result != TC_TLV_OK) return result;
      if (!valid) { *accepted = 0; return TC_TLV_OK; }
    }
  }
  *accepted = 1;
  return TC_TLV_OK;
}
TC_TLV_result tc_x509_policy_controls_read(const TC_X509_certificate* certificate,
    const TC_TLV_limits* limits, size_t* work, tc_x509_policy_controls* out)
{
  tc_x509_policy_controls controls;
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result;
  int has_constraints = 0;
  if (!certificate || !limits || !work || !out) return TC_TLV_ARGUMENT;
  memset(&controls, 0, sizeof controls);
  result = tc_pki_extensions_init(&reader, certificate, limits, work);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
    unsigned id = tc_pki_extension_id(&extension);
    if (id == 36) {
      if (has_constraints) return TC_TLV_INVALID;
      has_constraints = 1;
      result = TC_X509_policy_constraints_read(extension.value.data, extension.value.length, &controls.constraints);
      if (result != TC_TLV_OK) return result;
    } else if (id == 54) {
      if (controls.has_inhibit_any) return TC_TLV_INVALID;
      controls.has_inhibit_any = 1;
      result = TC_DER_uint32(extension.value.data, extension.value.length, &controls.inhibit_any);
      if (result != TC_TLV_OK) return result;
    }
  }
  if (result != TC_TLV_END) return result;
  *out = controls;
  return TC_TLV_OK;
}

void tc_x509_policy_counters_advance(tc_x509_policy_counters* counters,
    const tc_x509_policy_controls* controls, int self_issued, int target)
{
  const TC_X509_policy_constraints* constraints = &controls->constraints;
  if (target) {
    if (counters->explicit_policy) --counters->explicit_policy;
    if (constraints->has_require_explicit_policy && !constraints->require_explicit_policy)
      counters->explicit_policy = 0;
    return;
  }
  if (!self_issued) {
    if (counters->explicit_policy) --counters->explicit_policy;
    if (counters->mapping) --counters->mapping;
    if (counters->any) --counters->any;
  }
  if (constraints->has_require_explicit_policy
      && constraints->require_explicit_policy < counters->explicit_policy)
    counters->explicit_policy = constraints->require_explicit_policy;
  if (constraints->has_inhibit_policy_mapping && constraints->inhibit_policy_mapping < counters->mapping)
    counters->mapping = constraints->inhibit_policy_mapping;
  if (controls->has_inhibit_any && controls->inhibit_any < counters->any)
    counters->any = controls->inhibit_any;
}
static TC_TLV_result certificate_policies(const TC_X509_certificate* certificate,
    const TC_TLV_limits* limits, const tc_x509_policy_workspace* workspace,
    size_t* work, size_t* policy_count, size_t* mapping_count)
{
  TC_TLV_reader reader;
  TC_X509_extension extension;
  TC_TLV_result result;
  int has_policies = 0, has_mappings = 0;
  *policy_count = *mapping_count = 0;
  result = tc_pki_extensions_init(&reader, certificate, limits, work);
  if (result != TC_TLV_OK) return result;
  while ((result = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
    unsigned id = tc_pki_extension_id(&extension);
    if (id == 32) {
      TC_X509_policy_reader policies;
      TC_X509_policy policy;
      if (has_policies) return TC_TLV_INVALID;
      has_policies = 1;
      result = TC_X509_policies_init(&policies, extension.value.data, extension.value.length,
        limits, workspace->policies, workspace->policy_capacity);
      if (result != TC_TLV_OK) return result;
      for (;;) {
        /* Includes the decoder's comparisons against previously seen OIDs. */
        if (tc_x509_path_charge(work, extension.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
        result = TC_X509_policy_next(&policies, &policy);
        if (result != TC_TLV_OK) break;
        result = tc_x509_policy_qualifiers_check(&policy, extension.critical, limits,
          workspace->frames, workspace->frame_capacity, work);
        if (result != TC_TLV_OK) return result;
      }
      if (result != TC_TLV_END) return result;
      *policy_count = policies.count;
    } else if (id == 33) {
      TC_TLV_reader mappings;
      TC_X509_policy_mapping mapping;
      if (has_mappings) return TC_TLV_INVALID;
      has_mappings = 1;
      result = TC_X509_policy_mappings_init(&mappings, extension.value.data, extension.value.length, limits);
      if (result != TC_TLV_OK) return result;
      while ((result = TC_X509_policy_mapping_next(&mappings, &mapping)) == TC_TLV_OK) {
        if (tc_x509_path_charge(work, extension.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
        if (*mapping_count == workspace->mapping_capacity) return TC_TLV_LIMIT;
        workspace->mappings[(*mapping_count)++] = mapping;
      }
      if (result != TC_TLV_END) return result;
    }
  }
  return result == TC_TLV_END ? TC_TLV_OK : result;
}

TC_TLV_result tc_x509_path_policies(const tc_x509_path_input* input,
    const tc_x509_policy_options* options, const tc_x509_policy_workspace* workspace,
    size_t* work, size_t* count, int* accepted)
{
  tc_x509_policy_counters counters;
  TC_TLV_result result;
  size_t i, output_count;
  if (!path_source_valid(input) || !options || !workspace || !workspace->graph || !workspace->names
      || !work || !count || !accepted || !input->limits || !input->count
      || (workspace->policy_capacity && !workspace->policies)
      || (workspace->mapping_capacity && !workspace->mappings)
      || (workspace->output_capacity && !workspace->output)
      || (options->initial_count && !options->initial)) return TC_TLV_ARGUMENT;
  if (input->count > input->max_certificates || input->count == SIZE_MAX) return TC_TLV_LIMIT;
  counters.explicit_policy = options->require_explicit ? 0 : input->count + 1;
  counters.mapping = options->inhibit_mapping ? 0 : input->count + 1;
  counters.any = options->inhibit_any ? 0 : input->count + 1;
  result = tc_x509_policy_graph_init(workspace->graph);
  if (result != TC_TLV_OK) return result;
  for (i = 0; i < input->count; ++i) {
    TC_X509_certificate scratch;
    const TC_X509_certificate* certificate;
    tc_x509_policy_controls controls;
    size_t policy_count, mapping_count;
    int self_issued, target = i + 1 == input->count;
    result = path_certificate(input, i, &scratch, work, &certificate);
    if (result != TC_TLV_OK) return result;
    result = TC_X509_name_equal(certificate->subject, certificate->issuer,
      input->limits, workspace->names, work, &self_issued);
    if (result != TC_TLV_OK) return result;
    result = certificate_policies(certificate, input->limits, workspace, work, &policy_count, &mapping_count);
    if (result != TC_TLV_OK) return result;
    result = tc_x509_policy_controls_read(certificate, input->limits, work, &controls);
    if (result != TC_TLV_OK) return result;
    result = tc_x509_policy_graph_step(workspace->graph, workspace->policies, policy_count,
      NULL, 0, counters.any > 0 || (self_issued && !target), 0, work);
    if (result != TC_TLV_OK) return result;
    if (!counters.explicit_policy && !workspace->graph->nodes[0].alive) {
      *accepted = 0; *count = 0; return TC_TLV_OK;
    }
    if (!target) {
      result = tc_x509_policy_graph_map(workspace->graph, workspace->mappings, mapping_count,
        counters.mapping > 0, work);
      if (result != TC_TLV_OK) return result;
    }
    tc_x509_policy_counters_advance(&counters, &controls, self_issued, target);
  }
  result = tc_x509_policy_graph_output(workspace->graph, options->initial, options->initial_count,
    workspace->output, workspace->output_capacity, work, &output_count);
  if (result != TC_TLV_OK) return result;
  *count = output_count;
  *accepted = counters.explicit_policy > 0 || output_count > 0;
  return TC_TLV_OK;
}
TC_TLV_result tc_x509_path_extensions(const tc_x509_path_input* input,
    const tc_x509_path_usage* usage, const tc_x509_extension_workspace* workspace,
    size_t* work, int* accepted)
{
  static const uint8_t any_eku[] = {0x55,0x1d,0x25,0};
  const TC_X509_name_constraints unrestricted = {{NULL,0},{NULL,0}};
  size_t i;
  if (!path_source_valid(input) || !usage || !workspace || !work || !accepted
      || !input->limits || !input->count || (usage->purpose.length && !usage->purpose.data)
      || (usage->require_extended_key_usage && !usage->purpose.length)
      || (usage->key_usage & ~(unsigned)TC_KEY_USAGE_ALL) || (workspace->oid_capacity && !workspace->oids))
    return TC_TLV_ARGUMENT;
  if (input->count > input->max_certificates) return TC_TLV_LIMIT;
  for (i = 0; i < input->count; ++i) {
    TC_X509_certificate scratch;
    const TC_X509_certificate* certificate;
    TC_TLV_reader reader;
    TC_X509_extension extension;
    TC_X509_basic_constraints basic;
    TC_TLV_result result;
    uint16_t key_usage = 0;
    int has_usage = 0, has_basic = 0, has_eku = 0, has_constraints = 0, valid;
    result = path_certificate(input, i, &scratch, work, &certificate);
    if (result != TC_TLV_OK) return result;
    memset(&basic, 0, sizeof basic);
    result = TC_X509_certificate_names_check(certificate, &unrestricted,
      input->limits, &workspace->names, work, &valid);
    if (result != TC_TLV_OK) return result;
    if (!valid) { *accepted = 0; return TC_TLV_OK; }
    result = tc_pki_extensions_init(&reader, certificate, input->limits, work);
    if (result != TC_TLV_OK) return result;
    while ((result = tc_pki_extension_next(&reader, work, &extension)) == TC_TLV_OK) {
      unsigned id = tc_pki_extension_id(&extension);
      if (id == 15 || id == 17 || id == 19 || id == 30 || id == 32 || id == 33 || id == 36 || id == 37 || id == 54) {
        if (tc_x509_path_charge(work, extension.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
        result = TC_TLV_walk(extension.value.data, extension.value.length, TC_TLV_DER,
          input->limits, workspace->names.frames, workspace->names.frame_capacity, NULL, NULL);
        if (result != TC_TLV_OK) return result;
      }
      switch (id) {
        case 19:
          if (has_basic) return TC_TLV_INVALID;
          has_basic = 1;
          result = TC_X509_basic_constraints_read(extension.value.data, extension.value.length, &basic);
          if (result != TC_TLV_OK) return result;
          break;
        case 15:
          result = tc_pki_key_usage_value(extension.value,&has_usage,&key_usage);
          if (result != TC_TLV_OK) return result;
          break;
        case 37: {
          size_t j, count;
          int permitted = !usage->purpose.length;
          if (has_eku) return TC_TLV_INVALID;
          has_eku = 1;
          if (tc_x509_path_charge(work, extension.value.length) != TC_TLV_OK) return TC_TLV_LIMIT;
          result = TC_X509_extended_key_usage_read(extension.value.data, extension.value.length,
            workspace->oids, workspace->oid_capacity, &count);
          if (result != TC_TLV_OK) return result;
          if (count > input->limits->max_elements) return TC_TLV_LIMIT;
          for (j = 0; j < count; ++j) {
            TC_bytes oid = workspace->oids[j];
            if (tc_x509_path_charge(work, oid.length) != TC_TLV_OK) return TC_TLV_LIMIT;
            if ((!usage->inhibit_any_purpose && oid.length == sizeof any_eku && !memcmp(oid.data, any_eku, sizeof any_eku))
                || tc_pki_equal(oid, usage->purpose)) permitted = 1;
          }
          if (!permitted) { *accepted = 0; return TC_TLV_OK; }
          break;
        }
        case 30: {
          TC_X509_name_constraints constraints;
          if (has_constraints) return TC_TLV_INVALID;
          has_constraints = 1;
          result = TC_X509_name_constraints_read(extension.value.data, extension.value.length, input->limits, &constraints);
          if (result != TC_TLV_OK) return result;
          result = constraint_distances(&constraints, input->limits, &workspace->names, work);
          if (result != TC_TLV_OK) return result;
          break;
        }
        case 17: /* Subject names were checked above, including unconstrained paths. */
        case 32: case 33: case 36: case 54: /* Processed by the policy pass. */
          break;
        default:
          if (extension.critical) return TC_TLV_UNSUPPORTED;
          break;
      }
    }
    if (result != TC_TLV_END) return result;
    if ((has_constraints || (key_usage & TC_KEY_USAGE_CERT_SIGN)) && !basic.ca) return TC_TLV_INVALID;
    if (i + 1 == input->count) {
      if ((usage->require_key_usage && !has_usage) || (usage->require_extended_key_usage && !has_eku)
          || (has_usage && (key_usage & usage->key_usage) != usage->key_usage)) {
        *accepted = 0; return TC_TLV_OK;
      }
    }
  }
  *accepted = 1;
  return TC_TLV_OK;
}
TC_TLV_result tc_x509_path_storage_writes(const TC_X509_path_workspace* workspace,
    TC_bytes writes[TC_X509_PATH_STORAGE_COUNT])
{
  TC_TLV_result result;
#define PATH_STORAGE(slot, pointer, count) do { \
  result = tc_pki_storage_span((pointer), (count), sizeof *(pointer), &writes[slot]); \
  if (result != TC_TLV_OK) return result; \
} while (0)
  PATH_STORAGE(TC_X509_PATH_STORAGE_FRAMES, workspace->frames, workspace->frame_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_OIDS, workspace->oids, workspace->oid_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_NAME_LEFT, workspace->names.left, workspace->names.scalar_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_NAME_RIGHT, workspace->names.right, workspace->names.scalar_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_NAME_MATCHED, workspace->names.matched, workspace->names.attribute_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_NODES, workspace->nodes, workspace->node_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_EDGES, workspace->edges, workspace->edge_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_EXPECTED, workspace->expected, workspace->expected_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_MAPPINGS, workspace->mappings, workspace->mapping_capacity);
  PATH_STORAGE(TC_X509_PATH_STORAGE_POLICIES, workspace->policies, workspace->policy_capacity);
#undef PATH_STORAGE
  return TC_TLV_OK;
}

static TC_TLV_result path_storage(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_path_options* options,
    const TC_X509_path_workspace* workspace, TC_X509_path_result* out, size_t* work)
{
  TC_bytes writes[TC_X509_PATH_STORAGE_COUNT + 1], inputs[15];
  TC_TLV_result result = tc_x509_path_storage_writes(workspace,writes);
  size_t i, j, n = TC_X509_PATH_STORAGE_COUNT + 1;
  if (result != TC_TLV_OK) return result;
  result = tc_pki_storage_span(out,1,sizeof *out,&writes[TC_X509_PATH_STORAGE_COUNT]);
  if (result != TC_TLV_OK) return result;
  for (i = 0; i < n; ++i) for (j = 0; j < i; ++j) {
    if (tc_x509_path_charge(work, 1) != TC_TLV_OK) return TC_TLV_LIMIT;
    if (!tc_internal_ranges_disjoint(writes[i].data, writes[i].length, writes[j].data, writes[j].length))
      return TC_TLV_ARGUMENT;
  }
  result = tc_pki_storage_span(chain, count, sizeof *chain, &inputs[0]);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_storage_span(anchor, 1, sizeof *anchor, &inputs[1]);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_storage_span(options, 1, sizeof *options, &inputs[2]);
  if (result != TC_TLV_OK) return result;
  result = tc_pki_storage_span(workspace, 1, sizeof *workspace, &inputs[3]);
  if (result != TC_TLV_OK) return result;
  inputs[4] = anchor->name;
  inputs[5] = anchor->public_key.algorithm.oid;
  inputs[6] = anchor->public_key.algorithm.parameters;
  inputs[7] = anchor->public_key.key;
  inputs[8] = anchor->public_key.modulus;
  inputs[9] = anchor->public_key.exponent;
  inputs[10] = anchor->public_key.curve_oid;
  result = tc_pki_storage_span(options->initial_policies, options->initial_policy_count,
    sizeof *options->initial_policies, &inputs[11]);
  if (result != TC_TLV_OK) return result;
  inputs[12] = options->anchor_names.permitted;
  inputs[13] = options->anchor_names.excluded;
  inputs[14] = options->purpose;
  for (i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
    result = tc_pki_storage_input(writes, n, inputs[i], work);
    if (result != TC_TLV_OK) return result;
  }
  for (i = 0; i < count; ++i) {
    result = tc_pki_storage_input(writes, n, chain[i], work);
    if (result != TC_TLV_OK) return result;
  }
  for (i = 0; i < options->initial_policy_count; ++i) {
    result = tc_pki_storage_input(writes, n, options->initial_policies[i], work);
    if (result != TC_TLV_OK) return result;
  }
  return TC_TLV_OK;
}

TC_X509_path_status tc_x509_path_validate_anchor(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_name_constraints* anchor_names,
    const TC_X509_path_options* options,
    const TC_X509_path_workspace* workspace, size_t* work, TC_X509_path_result* out)
{
  TC_X509_workspace parser;
  TC_X509_constraint_workspace names;
  tc_x509_path_input input;
  tc_x509_policy_graph graph;
  tc_x509_policy_options policy_options;
  tc_x509_policy_workspace policy_workspace;
  tc_x509_path_usage usage;
  tc_x509_extension_workspace extension_workspace;
  TC_X509_certificate scratch;
  const TC_X509_certificate* target;
  TC_X509_path_result validated;
  TC_TLV_result result;
  size_t i, set, initial_work, total = 0, policy_count;
  int accepted;
  if (!chain || !anchor || !options || !workspace || !work || !out ||
      (options->flags & ~(unsigned)TC_X509_PATH_SUPPORTED_FLAGS)) return TC_X509_PATH_ERROR;
  if (!count) return TC_X509_PATH_INVALID;
  if (count > options->max_certificates || count > SIZE_MAX / sizeof *chain
      || options->initial_policy_count > options->parsing.max_elements) return TC_X509_PATH_LIMIT;
  initial_work = *work;
  result = path_storage(chain, count, anchor, options, workspace, out, work);
  if (result != TC_TLV_OK) return tc_x509_path_status(result);
  if (TC_X509_time_compare(&options->at, &options->at, &accepted) != TC_TLV_OK) return TC_X509_PATH_ERROR;
  for (i = 0; i < count; ++i) {
    if (tc_x509_path_charge(work, 1) != TC_TLV_OK) return TC_X509_PATH_LIMIT;
    if (!chain[i].length) return TC_X509_PATH_INVALID;
    if (chain[i].length > options->max_input - total) return TC_X509_PATH_LIMIT;
    total += chain[i].length;
  }
  for (i = 0; i < options->initial_policy_count; ++i) {
    TC_bytes oid = options->initial_policies[i];
    if (tc_x509_path_charge(work, oid.length) != TC_TLV_OK) return TC_X509_PATH_LIMIT;
    if (TC_DER_oid_contents(oid.data, oid.length) != TC_TLV_OK) return TC_X509_PATH_ERROR;
  }
  if (options->purpose.length) {
    if (tc_x509_path_charge(work, options->purpose.length) != TC_TLV_OK) return TC_X509_PATH_LIMIT;
    if (TC_DER_oid_contents(options->purpose.data, options->purpose.length) != TC_TLV_OK) return TC_X509_PATH_ERROR;
  }
  parser.frames = workspace->frames; parser.frame_capacity = workspace->frame_capacity;
  parser.extension_oids = workspace->oids; parser.extension_capacity = workspace->oid_capacity;
  names.frames = workspace->frames; names.frame_capacity = workspace->frame_capacity; names.names = &workspace->names;
  memset(&input, 0, sizeof input);
  input.count = count; input.max_certificates = options->max_certificates; input.max_input = options->max_input;
  input.anchor = anchor; input.at = &options->at; input.signatures = &options->signatures;
  input.limits = &options->parsing; input.encoded = chain; input.parser = &parser;
  result = tc_x509_path_basic(&input, &workspace->names, work, &accepted);
  if (result != TC_TLV_OK) return tc_x509_path_status(result);
  if (!accepted) return TC_X509_PATH_INVALID;
  result = tc_x509_path_names(&input, &names, work, &accepted);
  if (result != TC_TLV_OK) return tc_x509_path_status(result);
  if (!accepted) return TC_X509_PATH_INVALID;
  /* Apply application and stored-anchor constraints independently. */
  for (set = 0; set < 2; ++set) {
    const TC_X509_name_constraints* constraints = set ? anchor_names : &options->anchor_names;
    if (!constraints || (!constraints->permitted.length && !constraints->excluded.length)) continue;
    for (i = 0; i < count; ++i) {
      result = path_certificate(&input, i, &scratch, work, &target);
      if (result != TC_TLV_OK) return tc_x509_path_status(result);
      if (i + 1 < count) {
        result = TC_X509_name_equal(target->subject, target->issuer, input.limits, &workspace->names, work, &accepted);
        if (result != TC_TLV_OK) return tc_x509_path_status(result);
        if (accepted) continue;
      }
      result = TC_X509_certificate_names_check(target, constraints, input.limits, &names, work, &accepted);
      if (result != TC_TLV_OK) return tc_x509_path_status(result);
      if (!accepted) return TC_X509_PATH_INVALID;
    }
  }
  memset(&graph, 0, sizeof graph);
  graph.nodes = workspace->nodes; graph.node_capacity = workspace->node_capacity;
  graph.edges = workspace->edges; graph.edge_capacity = workspace->edge_capacity;
  graph.expected = workspace->expected; graph.expected_capacity = workspace->expected_capacity;
  policy_options.initial = options->initial_policies; policy_options.initial_count = options->initial_policy_count;
  policy_options.require_explicit = (options->flags & TC_X509_PATH_REQUIRE_EXPLICIT_POLICY) != 0;
  policy_options.inhibit_mapping = (options->flags & TC_X509_PATH_INHIBIT_MAPPING) != 0;
  policy_options.inhibit_any = (options->flags & TC_X509_PATH_INHIBIT_ANY_POLICY) != 0;
  policy_workspace.graph = &graph; policy_workspace.policies = workspace->oids; policy_workspace.policy_capacity = workspace->oid_capacity;
  policy_workspace.mappings = workspace->mappings; policy_workspace.mapping_capacity = workspace->mapping_capacity;
  policy_workspace.output = workspace->policies; policy_workspace.output_capacity = workspace->policy_capacity;
  policy_workspace.names = &workspace->names; policy_workspace.frames = workspace->frames; policy_workspace.frame_capacity = workspace->frame_capacity;
  result = tc_x509_path_policies(&input, &policy_options, &policy_workspace, work, &policy_count, &accepted);
  if (result != TC_TLV_OK) return tc_x509_path_status(result);
  if (!accepted) return TC_X509_PATH_INVALID;
  usage.purpose = options->purpose; usage.key_usage = options->key_usage;
  usage.require_key_usage = (options->flags & TC_X509_PATH_REQUIRE_KEY_USAGE) != 0;
  usage.require_extended_key_usage = (options->flags & TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE) != 0;
  usage.inhibit_any_purpose = (options->flags & TC_X509_PATH_INHIBIT_ANY_PURPOSE) != 0;
  extension_workspace.oids = workspace->oids; extension_workspace.oid_capacity = workspace->oid_capacity;
  extension_workspace.names = names;
  result = tc_x509_path_extensions(&input, &usage, &extension_workspace, work, &accepted);
  if (result != TC_TLV_OK) return tc_x509_path_status(result);
  if (!accepted) return TC_X509_PATH_INVALID;
  result = path_certificate(&input, count - 1, &scratch, work, &target);
  if (result != TC_TLV_OK) return tc_x509_path_status(result);
  validated.public_key = target->public_key; validated.policies = workspace->policies;
  validated.policy_count = policy_count; validated.work_used = initial_work - *work;
  *out = validated;
  return TC_X509_PATH_VALID;
}

TC_X509_path_status tc_x509_path_validate_budget(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_path_options* options,
    const TC_X509_path_workspace* workspace, size_t* work, TC_X509_path_result* out)
{
  return tc_x509_path_validate_anchor(chain,count,anchor,NULL,options,workspace,work,out);
}

TC_X509_path_status TC_X509_path_validate(const TC_bytes* chain, size_t count,
    const TC_X509_trust_anchor* anchor, const TC_X509_path_options* options,
    const TC_X509_path_workspace* workspace, TC_X509_path_result* out)
{
  size_t work;
  if (!options) return TC_X509_PATH_ERROR;
  work = options->max_work;
  return tc_x509_path_validate_budget(chain, count, anchor, options, workspace, &work, out);
}
#endif
