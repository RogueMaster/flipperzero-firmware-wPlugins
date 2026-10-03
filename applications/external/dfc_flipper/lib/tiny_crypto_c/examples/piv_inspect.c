/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_inspect.h"
#include "piv_inspect_crl.h"
#include "piv_inspect_print.h"
#include "piv_inspect_trust.h"
#include <tiny_crypto/piv_sm_apdu.h>
#include <tiny_crypto/piv_sm_authenticate.h>
#include <tiny_crypto/piv_vci.h>
#include <string.h>

#define COMMAND_SCRATCH_BYTES                                                                      \
  (TC_PIV_EXTENDED_SCRATCH_BYTES > TC_APDU_SHORT_COMMAND_MAX_BYTES                                 \
       ? TC_PIV_EXTENDED_SCRATCH_BYTES                                                             \
       : TC_APDU_SHORT_COMMAND_MAX_BYTES)

enum {
  EXCHANGES = 4096,          /* C-RP budget of the whole run */
  OBJECT_BYTES = 4096,       /* one plain object answer, or the decoded signer */
  CERTIFICATE_BYTES = 16384, /* decoded card certificates of the check */
  LDS_BYTES = 4096,          /* decoded Security Object content */
  VALIDATION_WORK = 200000000,
  CHECK_WORK = 400000000
};

/* The example's acceptance: the card authentication key and path, its
 * revocation and the CHUID, all Always readable (Part 1 Tables 2 and 5). */
static const TC_PIV_check_requirement baseline[] = {{TC_PIV_CHECK_CERTIFICATE_PATH, 0x9e, 0},
                                                    {TC_PIV_CHECK_REVOCATION, 0, 0x0500},
                                                    {TC_PIV_CHECK_KEY_PROOF, 0x9e, 0},
                                                    {TC_PIV_CHECK_CHUID, 0, 0}};
/* On contact or with the VCI: the Security Object signature. */
static const TC_PIV_check_requirement virtual_contact[] = {{TC_PIV_CHECK_SECURITY_SIGNATURE, 0, 0}};
/* When the application offers secure messaging: the signer, the card CVC
 * and the plain copies (Part 2 4.1). */
static const TC_PIV_check_requirement secured[] = {
    {TC_PIV_CHECK_SM_SIGNER, 0, 0}, {TC_PIV_CHECK_SM_CVC, 0, 0}, {TC_PIV_CHECK_COPY_MATCH, 0, 0}};
/* With a verified PIN: the PIV authentication key, every signed digest and
 * the fingerprint and facial image signatures. */
static const TC_PIV_check_requirement pin_gated[] = {{TC_PIV_CHECK_KEY_PROOF, 0x9a, 0},
                                                     {TC_PIV_CHECK_SECURITY_DIGEST, 0, 0},
                                                     {TC_PIV_CHECK_BIOMETRIC, 0, 0x6010},
                                                     {TC_PIV_CHECK_BIOMETRIC, 0, 0x6030}};

static struct {
  TC_PIV_link link;
  TC_PIV_SM session;
  TC_PIV_SM_workspace sm_workspace;
  TC_PIV_SM_authentication_workspace authentication;
  uint8_t command_scratch[COMMAND_SCRATCH_BYTES];
  uint8_t sm_scratch[TC_PIV_SM_COMMAND_DATA_BYTES(TC_PIV_COMMAND_MAX_NC)];
  uint8_t response[OBJECT_BYTES];
  uint8_t key_response[TC_PIV_SM_KEY_RESPONSE_BYTES];
  uint8_t copy_bytes[2][OBJECT_BYTES];
  uint8_t signer_der[OBJECT_BYTES];
  TC_GZIP_workspace gzip;
  uint8_t pool[TC_PIV_INVENTORY_POOL_BYTES];
  TC_PIV_object objects[TC_PIV_CATALOG_PIV_OBJECTS];
  TC_PIV_inventory inventory;
  ExamplePIVInspectTrust trust;
  ExamplePIVInspectCRLs crls;
  TC_PIV_card_check_workspace check;
  uint8_t certificates[CERTIFICATE_BYTES];
  uint8_t lds[LDS_BYTES];
  TC_PIV_key_proof_workspace proof;
  TC_PIV_card_report report;
  TC_PIV_card_report twic_report;
} storage;

/* State that one step hands to the next. */
typedef struct {
  const ExamplePIVInspectOptions* options;
  FILE* out;
  TC_PIV_application application;
  TC_PIV_card_profile profile; /* the credential profile of the checks */
  TC_PIV_certificate signer;   /* 5FC122 read plain */
  TC_X509_validation_report signer_result;
  TC_PIV_CHUID chuid; /* 5FC102 read plain */
  TC_PIV_discovery discovery;
  TC_PIV_object copies[2]; /* plain copies for COPY_MATCH */
  size_t copy_count;
  TC_bytes card_cvc;
  int has_signer, has_chuid, has_discovery, pin_verified;
  /* A NEXGEN TWIC application whose checks the result requires, and their
   * outcome. */
  int twic_required, twic_accepted;
  int step_failed; /* a supplied PIN or pairing code was refused */
} Inspect;

enum { COPY_SIGNER, COPY_CHUID };

static TC_buffer response_buffer(void)
{
  return (TC_buffer){storage.response, sizeof storage.response};
}

static int options_valid(const ExamplePIVInspectOptions* options, TC_APDU_transport transport,
                         FILE* out)
{
  return options && out && transport.transmit &&
         (options->interface == TC_PIV_CONTACT || options->interface == TC_PIV_CONTACTLESS) &&
         (options->format == TC_APDU_SHORT || options->format == TC_APDU_EXTENDED) &&
         options->minimum_retries >= 2 && options->random.fill && options->anchors &&
         options->anchor_count && options->anchor_count <= EXAMPLE_PIV_INSPECT_ANCHORS &&
         (options->crls || !options->crl_count) && options->crl_count <= EXAMPLE_PIV_INSPECT_CRLS;
}

/* Read the inventory of the selected application and print it. */
static TC_PIV_result inventory_read(Inspect* inspect)
{
  memset(&storage.inventory, 0, sizeof storage.inventory);
  storage.inventory.objects = storage.objects;
  storage.inventory.capacity = TC_PIV_CATALOG_PIV_OBJECTS;
  size_t work = CHECK_WORK;
  const TC_PIV_result result =
      TC_PIV_inventory_read(&storage.link, NULL, (TC_buffer){storage.pool, sizeof storage.pool},
                            &work, &storage.inventory);
  if (result != TC_PIV_OK) {
    fprintf(inspect->out, "Inventory: %s\n", example_piv_result_text(result));
    return result;
  }
  example_piv_print_objects(inspect->out, &storage.inventory);
  for (size_t i = 0; inspect->options->dump && i < storage.inventory.count; ++i) {
    const TC_PIV_object* object = &storage.inventory.objects[i];
    if (object->state == TC_PIV_OBJECT_PRESENT && !(object->info->flags & TC_PIV_OBJECT_SECRET))
      inspect->options->dump(inspect->options->dump_context, object);
  }
  return TC_PIV_OK;
}

static TC_PIV_result check(Inspect* inspect, TC_PIV_card_report* report);
static int crls_prepare(const Inspect* inspect, const TC_PIV_inventory* inventory);

/* On a NEXGEN TWIC application: the CHUID, the 9E path and revocation and the
 * 9E key proof of the TWIC application (TWIC Part 2 v5 4.6, 5.3, 7.5). */
static const TC_PIV_check_requirement twic_nexgen[] = {{TC_PIV_CHECK_CERTIFICATE_PATH, 0x9e, 0},
                                                       {TC_PIV_CHECK_REVOCATION, 0, 0x0500},
                                                       {TC_PIV_CHECK_KEY_PROOF, 0x9e, 0},
                                                       {TC_PIV_CHECK_CHUID, 0, 0}};

/* Check the TWIC inventory and print its report. A Legacy TWIC application
 * holds no card authentication certificate, so its report is informational. */
static int twic_check(Inspect* inspect)
{
  FILE* out = inspect->out;
  if (!crls_prepare(inspect, &storage.inventory))
    return 0;
  const TC_PIV_result checked = check(inspect, &storage.twic_report);
  if (checked != TC_PIV_OK) {
    fprintf(out, "TWIC card check: %s\n", example_piv_result_text(checked));
    return 0;
  }
  fputs("TWIC application checks:\n", out);
  example_piv_print_certificates(out, &storage.twic_report);
  example_piv_print_report(out, &storage.twic_report);
  inspect->twic_required = inspect->profile == TC_TWIC_NEXGEN_CARD;
  int failed = 0;
  for (size_t i = 0; i < storage.twic_report.count; ++i)
    failed |= storage.twic_report.checks[i].outcome == TC_PIV_CHECK_FAILED;
  inspect->twic_accepted =
      !failed && (!inspect->twic_required ||
                  TC_PIV_card_report_accepts(&storage.twic_report, twic_nexgen,
                                             sizeof twic_nexgen / sizeof *twic_nexgen));
  return 1;
}

/* Select the TWIC application, inventory it plain under its catalog and
 * check it. A TWIC card makes the checks of the PIV application use its TWIC
 * profile. */
static int twic_inspect(Inspect* inspect)
{
  TC_PIV_application twic;
  memset(&twic, 0, sizeof twic);
  const TC_PIV_result result =
      TC_PIV_select(&storage.link, TC_PIV_APPLICATION_TWIC, 0, response_buffer(), &twic);
  if (result == TC_PIV_CARD_STATUS) {
    fprintf(inspect->out, "TWIC application: absent (sw %04x)\n",
            TC_PIV_link_status(&storage.link));
    return 1;
  }
  if (result != TC_PIV_OK) {
    fprintf(inspect->out, "TWIC application: %s\n", example_piv_result_text(result));
    return result != TC_PIV_ERROR;
  }
  example_piv_print_application(inspect->out, "TWIC", &twic);
  inspect->profile = twic.profile;
  const TC_PIV_result read = inventory_read(inspect);
  const int checked = read == TC_PIV_OK ? twic_check(inspect) : read != TC_PIV_ERROR;
  TC_PIV_inventory_clear(&storage.inventory);
  return checked;
}

/* GET DATA tag into copy, as a catalog object. */
static int plain_read(Inspect* inspect, const uint8_t tag[3], size_t index)
{
  TC_PIV_data_object object;
  memset(&object, 0, sizeof object);
  const TC_bytes name = {tag, 3};
  if (TC_PIV_get_data(&storage.link, name,
                      (TC_buffer){storage.copy_bytes[index], sizeof storage.copy_bytes[index]},
                      &object) != TC_PIV_OK)
    return 0;
  TC_PIV_object* copy = &inspect->copies[index];
  memset(copy, 0, sizeof *copy);
  copy->info = TC_PIV_catalog_find(TC_PIV_APPLICATION_PIV, TC_PIV_CARD, name);
  copy->encoded = object.encoded;
  copy->value = object.value;
  copy->status = object.status;
  copy->state = TC_PIV_OBJECT_PRESENT;
  return copy->info != NULL;
}

/* Prepare the CRLs for the certificates of inventory and print a failure.
 * Returns 1 when the trust context holds them. */
static int crls_prepare(const Inspect* inspect, const TC_PIV_inventory* inventory)
{
  if (example_piv_inspect_crls_prepare(&storage.crls, inspect->options, inventory, &storage.trust))
    return 1;
  fputs("CRLs: unreadable or malformed\n", inspect->out);
  return 0;
}

/* Prepare the CRLs for the secure messaging signer alone, before secure
 * messaging reads the rest. */
static int signer_crls_prepare(Inspect* inspect)
{
  TC_PIV_inventory signer;
  memset(&signer, 0, sizeof signer);
  signer.objects = &inspect->copies[COPY_SIGNER];
  signer.capacity = signer.count = 1;
  signer.link.application = TC_PIV_APPLICATION_PIV;
  return crls_prepare(inspect, &signer);
}

/* Read the secure messaging signer 5FC122 and the CHUID in plaintext, both
 * Always readable (Part 1 Table 2), and validate the signer. Key
 * establishment binds the card CVC to both. */
static void plain_reads(Inspect* inspect)
{
  static const uint8_t signer_tag[] = {0x5f, 0xc1, 0x22}, chuid_tag[] = {0x5f, 0xc1, 0x02};
  const TC_validation_context* context = &storage.trust.context;
  size_t work = VALIDATION_WORK;
  if (plain_read(inspect, signer_tag, COPY_SIGNER) &&
      TC_PIV_certificate_decode(inspect->copies[COPY_SIGNER].encoded, TC_PIV_CERTIFICATE_SM_SIGNER,
                                sizeof storage.signer_der, &storage.gzip, &work,
                                (TC_buffer){storage.signer_der, sizeof storage.signer_der},
                                &inspect->signer) == TC_TLV_OK &&
      signer_crls_prepare(inspect)) {
    const TC_credential_status status = TC_PIV_content_signer_validate(
        inspect->signer.certificate, inspect->profile, context, &work, &inspect->signer_result);
    inspect->has_signer = status == TC_CREDENTIAL_VALID;
    fprintf(inspect->out, "SM certificate signer: %s", example_piv_status_text(status));
    fputs(inspect->has_signer && !inspect->signer_result.revocation_checked
              ? ", revocation unchecked\n"
              : "\n",
          inspect->out);
  } else {
    fputs("SM certificate signer: unavailable\n", inspect->out);
  }
  TC_PIV_CHUID_profile chuid_profile;
  inspect->has_chuid =
      TC_PIV_card_chuid_profile(TC_PIV_APPLICATION_PIV, inspect->profile, &chuid_profile) ==
          TC_TLV_OK &&
      plain_read(inspect, chuid_tag, COPY_CHUID) &&
      TC_PIV_CHUID_read(inspect->copies[COPY_CHUID].encoded, TC_PIV_CHUID_CONTAINER, chuid_profile,
                        &inspect->chuid) == TC_TLV_OK &&
      inspect->chuid.card_uuid.length == 16;
}

/* Key establishment, CVC authentication under the validated signer and the
 * CHUID GUID, then link securing (Part 2 4.1). */
static void secure(Inspect* inspect)
{
  const uint8_t suite = inspect->application.sm_suite;
  if (!suite || !inspect->has_signer || !inspect->has_chuid) {
    fprintf(inspect->out, "Secure messaging: %s\n",
            suite ? "no validated signer or CHUID" : "not offered");
    return;
  }
  TC_PIV_SM_peer peer;
  memset(&peer, 0, sizeof peer);
  TC_PIV_result result = TC_PIV_SM_key_request(
      &storage.link, &storage.session, (TC_PIV_SM_suite)suite, inspect->options->host_id,
      inspect->options->random, (TC_buffer){storage.key_response, sizeof storage.key_response},
      &peer, &storage.sm_workspace);
  if (result != TC_PIV_OK) {
    fprintf(inspect->out, "Secure messaging: key establishment %s\n",
            example_piv_result_text(result));
    TC_PIV_link_unsecure(&storage.link);
    return;
  }
  const TC_PIV_SM_authentication authentication = {peer,
                                                   inspect->signer.intermediate_cvc,
                                                   inspect->chuid.card_uuid,
                                                   &inspect->signer_result.certificate,
                                                   &storage.trust.options.parsing,
                                                   &storage.trust.options.signatures};
  size_t work = VALIDATION_WORK;
  const TC_credential_status status = TC_PIV_SM_authenticate_response(
      &storage.session, &authentication, &work, &storage.authentication);
  result = status == TC_CREDENTIAL_VALID
               ? TC_PIV_link_secure(&storage.link, &storage.sm_workspace,
                                    (TC_buffer){storage.sm_scratch, sizeof storage.sm_scratch})
               : TC_PIV_INVALID;
  if (result != TC_PIV_OK) {
    fprintf(inspect->out, "Secure messaging: card authentication %s\n",
            example_piv_status_text(status));
    TC_PIV_link_unsecure(&storage.link);
    return;
  }
  inspect->card_cvc = peer.certificate;
  inspect->copy_count = 2;
  fprintf(inspect->out, "Secure messaging: suite %02x established\n", suite);
}

/* The Discovery Object, under secure messaging when the link is secured,
 * then the VCI on contactless (Part 1 section 5.5). */
static void discovery_vci(Inspect* inspect)
{
  const TC_PIV_discovery_profile profile =
      inspect->profile == TC_PIV_CARD ? TC_PIV_DISCOVERY_PIV : TC_PIV_DISCOVERY_TWIC;
  inspect->has_discovery = TC_PIV_discovery_get(&storage.link, profile, response_buffer(),
                                                &inspect->discovery) == TC_PIV_OK;
  TC_PIV_link_info info;
  TC_PIV_link_info_get(&storage.link, &info);
  if (info.interface != TC_PIV_CONTACTLESS || !info.secured || !inspect->has_discovery ||
      !(inspect->discovery.policy & TC_PIV_POLICY_VCI))
    return;
  if (!(inspect->discovery.policy & TC_PIV_POLICY_VCI_WITHOUT_PAIRING) &&
      !inspect->options->pairing_code.length) {
    fputs("VCI: no pairing code\n", inspect->out);
    return;
  }
  TC_PIV_vci_mode mode = TC_PIV_VCI_PAIRED;
  const TC_PIV_result result = TC_PIV_vci_establish(&storage.link, &inspect->discovery,
                                                    inspect->options->pairing_code, &mode);
  if (result == TC_PIV_OK) {
    fputs(mode == TC_PIV_VCI_PAIRED ? "VCI: paired\n" : "VCI: without pairing\n", inspect->out);
    return;
  }
  inspect->step_failed = 1;
  fprintf(inspect->out, "VCI: %s (sw %04x)\n", example_piv_result_text(result),
          TC_PIV_link_status(&storage.link));
  TC_PIV_link_info_get(&storage.link, &info);
  if (info.sm_lost) {
    /* The session ended. Continue in plaintext. */
    TC_PIV_link_unsecure(&storage.link);
    inspect->card_cvc = (TC_bytes){NULL, 0};
    inspect->copy_count = 0;
  }
}

/* PIN verification with the reference the Discovery Object prefers, or a
 * retry query without a PIN. The library refuses both on contactless
 * without the VCI and keeps the retry floor. */
static void pin(Inspect* inspect)
{
  const uint8_t reference =
      inspect->has_discovery ? TC_PIV_discovery_pin_reference(&inspect->discovery) : 0x80;
  const TC_bytes value = inspect->options->pin;
  TC_PIV_reference_status status = {0, 0, 0, 0};
  const TC_PIV_result result = value.length
                                   ? TC_PIV_pin_verify(&storage.link, reference, value,
                                                       inspect->options->minimum_retries, &status)
                                   : TC_PIV_verify_status(&storage.link, reference, &status);
  fprintf(inspect->out, "PIN %02x: ", reference);
  inspect->step_failed |= value.length && !(result == TC_PIV_OK && status.verified);
  if (result == TC_PIV_OK && status.verified) {
    inspect->pin_verified = 1;
    fputs("verified\n", inspect->out);
  } else if (result == TC_PIV_OK) {
    fprintf(inspect->out, "not verified, %u tries left\n", status.retries);
  } else if (result == TC_PIV_CARD_STATUS) {
    fprintf(inspect->out, "card status %04x\n", TC_PIV_link_status(&storage.link));
  } else {
    fprintf(inspect->out, "%s\n", example_piv_result_text(result));
  }
}

/* The composed check and the key proofs of the PIV application. */
static TC_PIV_result check(Inspect* inspect, TC_PIV_card_report* report)
{
  const ExamplePIVInspectOptions* options = inspect->options;
  TC_PIV_card_check_ocsp ocsp;
  memcpy(ocsp.responses, options->ocsp, sizeof ocsp.responses);
  ocsp.max_responses = 16;
  ocsp.max_certificates = 4;
  const TC_PIV_card_check_request request = {
      &storage.inventory,     &storage.link,
      inspect->profile,       &storage.trust.context,
      &storage.trust.context, &ocsp,
      inspect->card_cvc,      inspect->copy_count ? inspect->copies : NULL,
      inspect->copy_count};
  storage.check.certificates = (TC_buffer){storage.certificates, sizeof storage.certificates};
  storage.check.lds_content = (TC_buffer){storage.lds, sizeof storage.lds};
  size_t work = CHECK_WORK;
  TC_PIV_result result = TC_PIV_card_check(&request, &storage.check, &work, report);
  if (result != TC_PIV_OK)
    return result;
  /* 9E is Always. 9A needs the PIN, and the VCI on contactless. */
  const unsigned keys = TC_PIV_CARD_PROVE_CARD_AUTHENTICATION |
                        (inspect->pin_verified ? TC_PIV_CARD_PROVE_PIV_AUTHENTICATION : 0u);
  const TC_PIV_card_proof_request proof = {keys,
                                           {inspect->profile, options->at, TC_PIV_RSA_PKCS1_V15,
                                            inspect->profile == TC_TWIC_LEGACY_CARD},
                                           options->random,
                                           &storage.trust.options.signatures};
  TC_work_budget budget = {UINT32_MAX};
  return TC_PIV_card_prove_keys(&storage.link, &proof, &storage.proof, &budget, report);
}

/* Accept when no step the inputs asked for failed, no entry FAILED and
 * every requirement of the tables that apply PASSED. */
static int accepted(const Inspect* inspect, const TC_PIV_card_report* report)
{
  if (inspect->step_failed || (inspect->twic_required && !inspect->twic_accepted))
    return 0;
  for (size_t i = 0; i < report->count; ++i)
    if (report->checks[i].outcome == TC_PIV_CHECK_FAILED)
      return 0;
  TC_PIV_link_info info;
  TC_PIV_link_info_get(&storage.link, &info);
  return TC_PIV_card_report_accepts(report, baseline, sizeof baseline / sizeof *baseline) &&
         ((info.interface == TC_PIV_CONTACTLESS && !info.vci) ||
          TC_PIV_card_report_accepts(report, virtual_contact,
                                     sizeof virtual_contact / sizeof *virtual_contact)) &&
         (!inspect->application.sm_suite ||
          TC_PIV_card_report_accepts(report, secured, sizeof secured / sizeof *secured)) &&
         (!inspect->pin_verified ||
          TC_PIV_card_report_accepts(report, pin_gated, sizeof pin_gated / sizeof *pin_gated));
}

/* The protocol order of docs/piv-card-check.md. Returns 0 accepted, 1
 * rejected or failed. */
static int inspect_card(Inspect* inspect, TC_APDU_transport transport, TC_PIV_card_report* report)
{
  const ExamplePIVInspectOptions* options = inspect->options;
  FILE* out = inspect->out;
  const TC_PIV_link_options link_options = {
      {options->format, 0, EXCHANGES, 0, 0},
      options->interface,
      options->format == TC_APDU_EXTENDED ? (uint32_t)TC_APDU_MAX_NE : 0};
  if (!example_piv_inspect_trust_init(&storage.trust, options)) {
    fputs("Trust: malformed anchor or CRL\n", out);
    return 1;
  }
  if (TC_PIV_link_init(&storage.link, transport, &link_options,
                       (TC_buffer){storage.command_scratch, sizeof storage.command_scratch}) !=
      TC_PIV_OK)
    return 1;
  fprintf(out, "Link: %s, %s length\n",
          options->interface == TC_PIV_CONTACT ? "contact" : "contactless",
          options->format == TC_APDU_SHORT ? "short" : "extended");
  inspect->profile = TC_PIV_CARD;
  if (!twic_inspect(inspect))
    return 1;
  const TC_PIV_result selected = TC_PIV_select(&storage.link, TC_PIV_APPLICATION_PIV, 0,
                                               response_buffer(), &inspect->application);
  if (selected != TC_PIV_OK) {
    fprintf(out, "PIV application: %s\n", example_piv_result_text(selected));
    return 1;
  }
  example_piv_print_application(out, "PIV", &inspect->application);
  plain_reads(inspect);
  secure(inspect);
  discovery_vci(inspect);
  pin(inspect);
  if (inventory_read(inspect) != TC_PIV_OK || !crls_prepare(inspect, &storage.inventory))
    return 1;
  const TC_PIV_result checked = check(inspect, report);
  if (checked != TC_PIV_OK) {
    fprintf(out, "Card check: %s\n", example_piv_result_text(checked));
    return 1;
  }
  example_piv_print_certificates(out, report);
  example_piv_print_report(out, report);
  const int accept = accepted(inspect, report);
  fputs(accept ? "Result: accepted\n" : "Result: rejected\n", out);
  return accept ? 0 : 1;
}

int example_piv_inspect_run(const ExamplePIVInspectOptions* options, TC_APDU_transport transport,
                            FILE* out, TC_PIV_card_report* report)
{
  if (!options_valid(options, transport, out))
    return 2;
  Inspect inspect;
  memset(&inspect, 0, sizeof inspect);
  inspect.options = options;
  inspect.out = out;
  const int status = inspect_card(&inspect, transport, report ? report : &storage.report);
  /* Every exit wipes the inventory, the link, the bound session and the
   * storage that held card data. */
  TC_PIV_inventory_clear(&storage.inventory);
  TC_PIV_link_clear(&storage.link);
  TC_PIV_SM_clear(&storage.session);
  example_piv_inspect_crls_clear(&storage.crls);
  TC_secure_zero(&storage, sizeof storage);
  TC_secure_zero(&inspect, sizeof inspect);
  return status;
}
