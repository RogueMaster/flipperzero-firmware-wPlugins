/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_pcsc.h"
#include <tiny_crypto/gzip.h>
#include <tiny_crypto/piv_certificate.h>
#include <tiny_crypto/piv_chuid.h>
#include <tiny_crypto/piv_command.h>
#include <tiny_crypto/x509.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>

enum {
  RESPONSE_CAPACITY = 16384,
  EXCHANGE_BUDGET = 512,
  FRAME_CAPACITY = 8,
  CERTIFICATE_CAPACITY = 8192,
  DECODE_WORK_LIMIT = 200000,
  EXTENSION_CAPACITY = 32
};
static struct {
  uint8_t response[RESPONSE_CAPACITY], certificate[CERTIFICATE_CAPACITY];
  uint8_t scratch[TC_APDU_SHORT_COMMAND_MAX_BYTES];
  TC_GZIP_workspace gzip;
} storage;

static TC_buffer response_buffer(void)
{
  return (TC_buffer){storage.response, sizeof storage.response};
}

static int inspect_certificate(TC_bytes input, TC_PIV_application_id application)
{
  TC_PIV_certificate container;
  const TC_PIV_certificate_profile profile =
      application == TC_PIV_APPLICATION_PIV ? TC_PIV_CERTIFICATE_SLOT : TC_PIV_CERTIFICATE_TWIC;
  size_t work = DECODE_WORK_LIMIT;
  if (TC_PIV_certificate_decode(input, profile, TC_PIV_CERTIFICATE_RECOMMENDED_BYTES, &storage.gzip,
                                &work, (TC_buffer){storage.certificate, sizeof storage.certificate},
                                &container) != TC_TLV_OK)
    return 0;
  const TC_bytes encoded = container.certificate;
  const TC_TLV_limits limits = {CERTIFICATE_CAPACITY, CERTIFICATE_CAPACITY, 512, FRAME_CAPACITY};
  TC_TLV_frame frames[FRAME_CAPACITY];
  TC_bytes extensions[EXTENSION_CAPACITY];
  const TC_X509_workspace workspace = {{frames, FRAME_CAPACITY}, extensions, EXTENSION_CAPACITY};
  TC_X509_certificate certificate;
  return TC_X509_read(encoded, &limits, &workspace, &certificate) == TC_TLV_OK;
}

/* 1 when the card completed command with a status meaning absent. */
static int absent(const TC_PIV_link* link, TC_PIV_result result, TC_PIV_command command,
                  TC_PIV_application_id application)
{
  return result == TC_PIV_CARD_STATUS &&
         TC_PIV_status_classify(TC_PIV_link_status(link), command, application, NULL) ==
             TC_PIV_SW_NOT_FOUND;
}

static int inspect_application(TC_PIV_link* link, TC_PIV_application_id application,
                               TC_PIV_CHUID_profile chuid_profile, int* found)
{
  enum { OBJECT_CHUID, OBJECT_CERTIFICATE };
  static const struct {
    const char* name;
    uint8_t tag[3];
    int kind;
  } objects[] = {{"CHUID", {0x5f, 0xc1, 0x02}, OBJECT_CHUID},
                 {"Card authentication certificate", {0x5f, 0xc1, 0x01}, OBJECT_CERTIFICATE}};
  const char* name = application == TC_PIV_APPLICATION_PIV ? "PIV" : "TWIC";
  TC_PIV_application selected;
  TC_PIV_result result = TC_PIV_select(link, application, 0, response_buffer(), &selected);
  if (absent(link, result, TC_PIV_COMMAND_SELECT, application)) {
    printf("%s: application absent\n", name);
    return 1;
  }
  if (result != TC_PIV_OK) {
    fprintf(stderr, "%s: selection failed or unsupported application identity\n", name);
    return 0;
  }
  ++*found;
  printf("%s: %s selected\n", name,
         selected.profile == TC_PIV_CARD           ? "PIV 1.0"
         : selected.profile == TC_TWIC_LEGACY_CARD ? "Legacy"
                                                   : "NEXGEN");
  TC_secure_zero(storage.response, sizeof storage.response);
  for (size_t i = 0; i < sizeof objects / sizeof *objects; ++i) {
    TC_PIV_data_object object;
    result = TC_PIV_get_data(link, (TC_bytes){objects[i].tag, sizeof objects[i].tag},
                             response_buffer(), &object);
    if (absent(link, result, TC_PIV_COMMAND_GET_DATA, application)) {
      printf("  %s: absent\n", objects[i].name);
      continue;
    }
    if (result != TC_PIV_OK) {
      fprintf(stderr, "  %s: read failed\n", objects[i].name);
      return 0;
    }
    TC_PIV_CHUID chuid;
    int valid = objects[i].kind == OBJECT_CHUID
                    ? TC_PIV_CHUID_read(object.encoded, TC_PIV_CHUID_CONTAINER, chuid_profile,
                                        &chuid) == TC_TLV_OK
                    : inspect_certificate(object.encoded, application);
    if (!valid) {
      fprintf(stderr, "  %s: malformed or oversized object\n", objects[i].name);
      return 0;
    }
    const char* authentication =
        objects[i].kind == OBJECT_CHUID && chuid_profile == TC_CHUID_PROFILE_TWIC_UNSIGNED
            ? "unsigned"
            : "signature unverified";
    printf("  %s: structure checked; %s\n", objects[i].name, authentication);
    TC_secure_zero(storage.response, sizeof storage.response);
    TC_secure_zero(storage.certificate, sizeof storage.certificate);
    TC_secure_zero(&storage.gzip, sizeof storage.gzip);
  }
  return 1;
}

int main(int argc, char** argv)
{
  if (argc == 2 && !strcmp(argv[1], "--help")) {
    puts("Usage: credential_check --reader NAME [--twic-unsigned-chuid]\n"
         "Select both applications and check CHUID/certificate structure.\n"
         "Reports status only. Credential signatures and trust are unverified.");
    return 0;
  }
  if ((argc != 3 && argc != 4) || strcmp(argv[1], "--reader") || !*argv[2] ||
      (argc == 4 && strcmp(argv[3], "--twic-unsigned-chuid"))) {
    fputs("Usage: credential_check --reader NAME [--twic-unsigned-chuid]\n", stderr);
    return 2;
  }
  /* Disable core files and lock the response storage before contacting a card. */
  const struct rlimit core_limit = {0, 0};
  if (setrlimit(RLIMIT_CORE, &core_limit) || mlock(&storage, sizeof storage)) {
    fputs("Unable to protect credential memory\n", stderr);
    return 1;
  }
  ExampleCardPCSC connection = {0};
  TC_PIV_link link;
  int found = 0, ok = 0;
  memset(&link, 0, sizeof link);
  const ExampleCardPCSCOptions reader = {argv[2], EXAMPLE_PCSC_DETECT, NULL};
  if (example_card_pcsc_open(&connection, &reader) != EXAMPLE_PCSC_OPENED) {
    fputs("Unable to acquire the reader transaction\n", stderr);
    goto cleanup;
  }
  /* No PIN is sent. The link takes the interface the ATR shows. The budget
   * covers both SELECTs, the GET DATA commands and their GET RESPONSE steps. */
  const TC_PIV_link_options options = {
      {TC_APDU_SHORT, 0, EXCHANGE_BUDGET, 0, 0}, example_card_pcsc_interface(&connection), 0};
  if (TC_PIV_link_init(&link, (TC_APDU_transport){example_card_pcsc_transmit, &connection},
                       &options, (TC_buffer){storage.scratch, sizeof storage.scratch}) != TC_PIV_OK)
    goto cleanup;
  const TC_PIV_CHUID_profile twic_profile =
      argc == 4 ? TC_CHUID_PROFILE_TWIC_UNSIGNED : TC_CHUID_PROFILE_TWIC_SIGNED;
  ok = inspect_application(&link, TC_PIV_APPLICATION_TWIC, twic_profile, &found) &&
       inspect_application(&link, TC_PIV_APPLICATION_PIV, TC_CHUID_PROFILE_PIV, &found) && found;
cleanup:
  TC_PIV_link_clear(&link);
  if (!example_card_pcsc_close(&connection)) {
    fputs("Reader cleanup failed\n", stderr);
    ok = 0;
  }
  TC_secure_zero(&storage, sizeof storage);
  if (munlock(&storage, sizeof storage))
    ok = 0;
  return ok ? 0 : 1;
}
