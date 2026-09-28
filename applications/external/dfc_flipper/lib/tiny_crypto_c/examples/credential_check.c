/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_pcsc.h"
#include <tiny_crypto/gzip.h>
#include <tiny_crypto/piv_certificate.h>
#include <tiny_crypto/piv_chuid.h>
#include <tiny_crypto/x509.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>

enum { RESPONSE_CAPACITY = 16384, EXCHANGE_BUDGET = 512, FRAME_CAPACITY = 8,
  CERTIFICATE_CAPACITY = 8192, DECODE_WORK_LIMIT = 200000, EXTENSION_CAPACITY = 32 };
static struct {
  uint8_t response[RESPONSE_CAPACITY], certificate[CERTIFICATE_CAPACITY];
  TC_GZIP_workspace gzip;
} storage;
#define response_buffer storage.response

static int inspect_certificate(size_t length, ExampleCardApplication application)
{
  TC_PIV_certificate container;
  const TC_bytes input = {response_buffer,length};
  const TC_PIV_certificate_profile profile = application == EXAMPLE_CARD_PIV ?
      TC_PIV_CERTIFICATE_SLOT : TC_PIV_CERTIFICATE_TWIC;
  if (TC_PIV_certificate_read(input,profile,&container) != TC_TLV_OK) return 0;
  TC_bytes encoded = container.certificate;
  if (container.compression == TC_PIV_CERTIFICATE_GZIP) {
    size_t work = DECODE_WORK_LIMIT, decoded;
    if (TC_GZIP_decode(encoded.data,encoded.length,storage.certificate,
        sizeof storage.certificate,&storage.gzip,&work,&decoded) != TC_GZIP_OK) return 0;
    encoded = (TC_bytes){storage.certificate,decoded};
  }
  const TC_TLV_limits limits = {CERTIFICATE_CAPACITY,CERTIFICATE_CAPACITY,512,FRAME_CAPACITY};
  TC_TLV_frame frames[FRAME_CAPACITY];
  TC_bytes extensions[EXTENSION_CAPACITY];
  TC_X509_workspace workspace = {frames,FRAME_CAPACITY,extensions,EXTENSION_CAPACITY};
  TC_X509_certificate certificate;
  return TC_X509_read(encoded.data,encoded.length,&limits,&workspace,&certificate) == TC_TLV_OK;
}

static int inspect_application(ExampleCardIO* io, ExampleCardApplication application,
    TC_PIV_CHUID_profile chuid_profile, int* found)
{
  enum { OBJECT_CHUID, OBJECT_CERTIFICATE };
  static const struct { const char* name; uint8_t tag[3]; int kind; } objects[] = {
    {"CHUID",{0x5f,0xc1,0x02},OBJECT_CHUID},
    {"Card authentication certificate",{0x5f,0xc1,0x01},OBJECT_CERTIFICATE}
  };
  const char* name = application == EXAMPLE_CARD_PIV ? "PIV" : "TWIC";
  ExampleCardResponse response;
  ExampleCardResult result = example_card_select(io,application,response_buffer,sizeof response_buffer,&response);
  if (result == EXAMPLE_CARD_STATUS && response.status == 0x6a82) {
    printf("%s: application absent\n",name);
    return 1;
  }
  if (result != EXAMPLE_CARD_OK) {
    fprintf(stderr,"%s: selection failed\n",name);
    return 0;
  }
  ExampleCardModel model;
  TC_bytes selection = {response_buffer,response.length};
  if (example_card_identity(selection,application,&model) != TC_TLV_OK) {
    fprintf(stderr,"%s: unsupported or malformed application identity\n",name);
    return 0;
  }
  ++*found;
  printf("%s: %s selected\n",name,model == EXAMPLE_CARD_MODEL_PIV ? "PIV 1.0" :
      model == EXAMPLE_CARD_MODEL_TWIC_LEGACY ? "Legacy" : "NEXGEN");
  TC_secure_zero(response_buffer,sizeof response_buffer);
  for (size_t i = 0; i < sizeof objects / sizeof *objects; ++i) {
    result = example_card_read(io,objects[i].tag,sizeof objects[i].tag,
        response_buffer,sizeof response_buffer,&response);
    if (result == EXAMPLE_CARD_STATUS && response.status == 0x6a88) {
      printf("  %s: absent\n",objects[i].name);
      TC_secure_zero(response_buffer,sizeof response_buffer);
      continue;
    }
    if (result != EXAMPLE_CARD_OK) {
      fprintf(stderr,"  %s: read failed\n",objects[i].name);
      return 0;
    }
    TC_PIV_CHUID chuid;
    int valid = objects[i].kind == OBJECT_CHUID ?
        TC_PIV_CHUID_read_profile(response_buffer,response.length,TC_PIV_CHUID_CONTAINER,
            chuid_profile,&chuid) == TC_TLV_OK : inspect_certificate(response.length,application);
    if (!valid) {
      fprintf(stderr,"  %s: malformed or oversized object\n",objects[i].name);
      return 0;
    }
    const char* authentication = objects[i].kind == OBJECT_CHUID &&
        chuid_profile == TC_CHUID_PROFILE_TWIC_UNSIGNED ? "unsigned" : "signature unverified";
    printf("  %s: structure checked; %s\n",objects[i].name,authentication);
    TC_secure_zero(&storage,sizeof storage);
  }
  return 1;
}

int main(int argc, char** argv)
{
  if (argc == 2 && !strcmp(argv[1],"--help")) {
    puts("Usage: credential_check --reader NAME [--twic-unsigned-chuid]\n"
         "Select both applications and check CHUID/certificate structure.\n"
         "Reports status only. Credential signatures and trust are unverified.");
    return 0;
  }
  if ((argc != 3 && argc != 4) || strcmp(argv[1],"--reader") || !*argv[2] ||
      (argc == 4 && strcmp(argv[3],"--twic-unsigned-chuid"))) {
    fputs("Usage: credential_check --reader NAME [--twic-unsigned-chuid]\n",stderr);
    return 2;
  }
  /* Disable core files and lock the response storage before contacting a card. */
  const struct rlimit core_limit = {0,0};
  if (setrlimit(RLIMIT_CORE,&core_limit) || mlock(&storage,sizeof storage)) {
    fputs("Unable to protect credential memory\n",stderr);
    return 1;
  }
  ExampleCardPCSC connection = {0};
  int found = 0, ok = 0;
  if (!example_card_pcsc_open(&connection,argv[2])) {
    fputs("Unable to acquire the reader transaction\n",stderr);
    goto cleanup;
  }
  ExampleCardIO io = {example_card_pcsc_transmit,&connection,EXCHANGE_BUDGET,0};
  const TC_PIV_CHUID_profile twic_profile = argc == 4 ?
      TC_CHUID_PROFILE_TWIC_UNSIGNED : TC_CHUID_PROFILE_TWIC_SIGNED;
  ok = inspect_application(&io,EXAMPLE_CARD_TWIC,twic_profile,&found) &&
      inspect_application(&io,EXAMPLE_CARD_PIV,TC_CHUID_PROFILE_PIV,&found) && found;
cleanup:
  if (!example_card_pcsc_close(&connection)) {
    fputs("Reader cleanup failed\n",stderr);
    ok = 0;
  }
  TC_secure_zero(&storage,sizeof storage);
  if (munlock(&storage,sizeof storage)) ok = 0;
  return ok ? 0 : 1;
}
