/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_inspect_print.h"
#include <tiny_crypto/piv_card_objects.h>
#include <tiny_crypto/piv_discovery.h>
#include <tiny_crypto/piv_printed.h>
#include <tiny_crypto/piv_security.h>

void example_piv_print_hex(FILE* out, TC_bytes bytes)
{
  for (size_t i = 0; i < bytes.length; ++i)
    fprintf(out, "%02x", bytes.data[i]);
}

void example_piv_print_text(FILE* out, TC_bytes bytes)
{
  for (size_t i = 0; i < bytes.length; ++i)
    fputc(bytes.data[i] >= 0x20 && bytes.data[i] < 0x7f ? bytes.data[i] : '.', out);
}

const char* example_piv_result_text(TC_PIV_result result)
{
  static const char* const names[] = {"ok",          "invalid", "limit",       "argument",
                                      "unsupported", "error",   "card status", "refused"};
  return (unsigned)result < sizeof names / sizeof *names ? names[result] : "unknown";
}

const char* example_piv_status_text(TC_credential_status status)
{
  static const char* const names[] = {"valid", "invalid", "revoked",    "unsupported",
                                      "limit", "error",   "unavailable"};
  return (unsigned)status < sizeof names / sizeof *names ? names[status] : "unknown";
}

/* Certificate names by key reference (SP 800-73-5 Part 1 Table 8). Retired
 * key management certificates use 82 to 95. */
static const char* certificate_name(uint8_t key_reference)
{
  static const char* const retired[] = {
      "Retired Key Management certificate 1",  "Retired Key Management certificate 2",
      "Retired Key Management certificate 3",  "Retired Key Management certificate 4",
      "Retired Key Management certificate 5",  "Retired Key Management certificate 6",
      "Retired Key Management certificate 7",  "Retired Key Management certificate 8",
      "Retired Key Management certificate 9",  "Retired Key Management certificate 10",
      "Retired Key Management certificate 11", "Retired Key Management certificate 12",
      "Retired Key Management certificate 13", "Retired Key Management certificate 14",
      "Retired Key Management certificate 15", "Retired Key Management certificate 16",
      "Retired Key Management certificate 17", "Retired Key Management certificate 18",
      "Retired Key Management certificate 19", "Retired Key Management certificate 20"};
  switch (key_reference) {
  case 0x9a:
    return "PIV Authentication certificate";
  case 0x9c:
    return "Digital Signature certificate";
  case 0x9d:
    return "Key Management certificate";
  case 0x9e:
    return "Card Authentication certificate";
  default:
    if (key_reference >= 0x82 && key_reference <= 0x95)
      return retired[key_reference - 0x82];
    return "Certificate";
  }
}

const char* example_piv_object_name(const TC_PIV_object_info* info)
{
  static const char* const names[] = {"Card Capability Container",
                                      "CHUID",
                                      "Unsigned CHUID",
                                      NULL,
                                      "Fingerprints",
                                      "Facial image",
                                      "Iris images",
                                      "Security Object",
                                      "Printed information",
                                      "Discovery Object",
                                      "Key History",
                                      "BIT group template",
                                      "Secure messaging certificate signer",
                                      "Pairing code",
                                      "TWIC Privacy Key",
                                      "TWIC personal data",
                                      "TWIC signature image"};
  if (!info)
    return "unknown";
  if (info->kind == TC_PIV_KIND_CERTIFICATE)
    return certificate_name(info->key_reference);
  return info->kind < sizeof names / sizeof *names ? names[info->kind] : "unknown";
}

void example_piv_print_application(FILE* out, const char* label,
                                   const TC_PIV_application* application)
{
  static const char* const profiles[] = {"PIV", "TWIC Legacy", "TWIC NEXGEN"};
  fprintf(out, "%s application: %s, version %02x %02x, AID ", label,
          (unsigned)application->profile < 3 ? profiles[application->profile] : "unknown",
          application->version[0], application->version[1]);
  example_piv_print_hex(out, application->aid);
  if (application->label.length) {
    fputs(", label \"", out);
    example_piv_print_text(out, application->label);
    fputc('"', out);
  }
  fputc('\n', out);
  if (application->sm_suite)
    fprintf(out, "  secure messaging suite %02x\n", application->sm_suite);
  if (application->max_command_bytes || application->max_response_bytes)
    fprintf(out, "  card limits: command %zu bytes, response %zu bytes\n",
            application->max_command_bytes, application->max_response_bytes);
}

static const char* state_text(uint8_t state)
{
  static const char* const names[] = {"present", "empty",     "absent", "restricted",
                                      "denied",  "oversized", "skipped"};
  return state < sizeof names / sizeof *names ? names[state] : "unknown";
}

static void print_entry(FILE* out, const TC_PIV_object* object)
{
  const TC_PIV_object_info* info = object->info;
  fputs("  ", out);
  example_piv_print_hex(out, (TC_bytes){info->tag, info->tag_length});
  fprintf(out, "%*s", 8 - 2 * info->tag_length, "");
  if (info->container)
    fprintf(out, "%04x  ", info->container);
  else
    fputs("----  ", out);
  fprintf(out, "%-10s", state_text(object->state));
  if (object->state == TC_PIV_OBJECT_PRESENT && (info->flags & TC_PIV_OBJECT_SECRET))
    fprintf(out, " %-11s", "secret");
  else if (object->state == TC_PIV_OBJECT_PRESENT)
    fprintf(out, " %5zu bytes", object->encoded.length);
  else if (object->state == TC_PIV_OBJECT_ABSENT || object->state == TC_PIV_OBJECT_DENIED)
    fprintf(out, " sw %04x   ", object->status);
  else
    fprintf(out, " %-11s", "");
  fprintf(out, " %s  %s\n", object->secured ? "sm" : "  ", example_piv_object_name(info));
}

static void print_policy(FILE* out, const TC_PIV_discovery* discovery)
{
  static const struct {
    uint8_t bit;
    const char* name;
  } bits[] = {{TC_PIV_POLICY_PIV_PIN, "PIV PIN"},
              {TC_PIV_POLICY_GLOBAL_PIN, "Global PIN"},
              {TC_PIV_POLICY_OCC, "OCC"},
              {TC_PIV_POLICY_VCI, "VCI"},
              {TC_PIV_POLICY_VCI_WITHOUT_PAIRING, "VCI without pairing"}};
  fprintf(out, "Discovery Object: policy %02x %02x", discovery->policy, discovery->preference);
  const char* separator = " (";
  for (size_t i = 0; i < sizeof bits / sizeof *bits; ++i)
    if (discovery->policy & bits[i].bit) {
      fprintf(out, "%s%s", separator, bits[i].name);
      separator = ", ";
    }
  fputs(*separator == ',' ? ")\n" : "\n", out);
}

/* The containers of inventory that the Security Object maps to data
 * groups. */
static void print_security(FILE* out, const TC_PIV_inventory* inventory, TC_bytes encoded)
{
  TC_PIV_security_object security;
  if (TC_PIV_security_read(encoded, TC_PIV_SECURITY_CONTAINER, &security) != TC_TLV_OK) {
    fputs("Security Object: malformed\n", out);
    return;
  }
  fputs("Security Object: container=group", out);
  for (size_t i = 0; i < inventory->count; ++i) {
    unsigned group = 0;
    const uint16_t container = inventory->objects[i].info->container;
    if (container && TC_PIV_security_group_find(&security, container, &group) == TC_TLV_OK)
      fprintf(out, " %04x=%u", container, group);
  }
  fputc('\n', out);
}

/* The contents of one present object, read with the library readers. */
static void print_details(FILE* out, const TC_PIV_inventory* inventory, const TC_PIV_object* object)
{
  const TC_bytes encoded = object->encoded;
  TC_PIV_CCC ccc;
  TC_PIV_discovery discovery;
  TC_PIV_key_history history;
  TC_PIV_bit_group bits;
  TC_PIV_printed printed;
  const TC_PIV_discovery_profile discovery_profile =
      inventory->link.profile == TC_PIV_CARD ? TC_PIV_DISCOVERY_PIV : TC_PIV_DISCOVERY_TWIC;
  switch (object->info->kind) {
  case TC_PIV_KIND_CCC:
    if (TC_PIV_CCC_read(encoded, TC_PIV_CONTAINER, &ccc) == TC_TLV_OK)
      fprintf(out, "Card Capability Container: data model %02x\n", ccc.data_model);
    break;
  case TC_PIV_KIND_DISCOVERY:
    if (TC_PIV_discovery_read(encoded, discovery_profile, &discovery) == TC_TLV_OK)
      print_policy(out, &discovery);
    break;
  case TC_PIV_KIND_KEY_HISTORY:
    if (TC_PIV_key_history_read(encoded, TC_PIV_CONTAINER, &history) == TC_TLV_OK) {
      fprintf(out, "Key History: %u on card, %u off card", history.on_card, history.off_card);
      if (history.url.data) {
        fputs(", ", out);
        example_piv_print_text(out, history.url);
      }
      fputc('\n', out);
    }
    break;
  case TC_PIV_KIND_BIT_GROUP:
    if (TC_PIV_bit_group_read(encoded, &bits) == TC_TLV_OK)
      fprintf(out, "BIT group template: %u fingers\n", bits.fingers);
    break;
  case TC_PIV_KIND_SECURITY:
    print_security(out, inventory, encoded);
    break;
  case TC_PIV_KIND_PRINTED:
    if (TC_PIV_printed_read(encoded, TC_PIV_PRINTED_CONTAINER,
                            inventory->link.profile == TC_PIV_CARD ? TC_PIV_PRINTED_PROFILE_PIV
                                                                   : TC_PIV_PRINTED_PROFILE_TWIC,
                            &printed) == TC_TLV_OK) {
      fputs("Printed information: expiration ", out);
      example_piv_print_text(out, printed.expiration_text);
      fputc('\n', out);
    }
    break;
  default:
    break;
  }
}

void example_piv_print_objects(FILE* out, const TC_PIV_inventory* inventory)
{
  fprintf(out, "Objects (%zu):\n", inventory->count);
  for (size_t i = 0; i < inventory->count; ++i)
    print_entry(out, &inventory->objects[i]);
  for (size_t i = 0; i < inventory->count; ++i) {
    const TC_PIV_object* object = &inventory->objects[i];
    if (object->state == TC_PIV_OBJECT_PRESENT && !(object->info->flags & TC_PIV_OBJECT_SECRET))
      print_details(out, inventory, object);
  }
}
