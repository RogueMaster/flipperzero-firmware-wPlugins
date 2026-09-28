/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "cms_validate.h"
#include "pki_input.h"
#include <tiny_crypto/x509_crypto.h>
#include <stdio.h>
#include <string.h>

enum { OBJECT_BYTES = 16384, CERTIFICATE_BYTES = 4096, CERTIFICATES = 2,
       CRLS = 2, ELEMENTS = 1024, MAX_RSA_BITS = 3072, WORK_LIMIT = 1000000 };

static int read_time(const char* text, TC_X509_time* out)
{
  enum { TIME_DIGITS = 14, COMPONENTS = 6 };
  unsigned fields[COMPONENTS] = {0};
  if (strlen(text) != TIME_DIGITS) return 0;
  for (size_t i = 0; i < TIME_DIGITS; ++i) {
    if (text[i] < '0' || text[i] > '9') return 0;
    size_t field = i < 4 ? 0 : 1 + (i - 4) / 2;
    fields[field] = fields[field] * 10 + (unsigned)(text[i] - '0');
  }
  TC_X509_time value = {fields[0],(uint8_t)fields[1],(uint8_t)fields[2],
    (uint8_t)fields[3],(uint8_t)fields[4],(uint8_t)fields[5]};
  int order;
  if (TC_X509_time_compare(&value,&value,&order) != TC_TLV_OK) return 0;
  *out = value;
  return 1;
}

int main(int argc, char** argv)
{
  if (argc != 7) {
    fprintf(stderr,"Usage: %s signed-data.der root.der issuer.der root.crl issuer.crl YYYYMMDDhhmmss\n",argv[0]);
    return 2;
  }
  /* Static storage keeps certificate and crypto workspaces off the task stack. */
  static uint8_t message[OBJECT_BYTES], certificates[CERTIFICATES][CERTIFICATE_BYTES];
  static uint8_t crl_bytes[CRLS][OBJECT_BYTES];
  static ExampleCMSCredentialWorkspace scratch;
  static TC_ECDSA_workspace ec;
  static TC_RSA_word rsa_words[TC_RSA_VERIFY_WORKSPACE_WORDS(MAX_RSA_BITS)];
  static TC_X509_crl_record records[CRLS];
  TC_bytes candidates[CERTIFICATES];
  TC_X509_store_anchor trusted_anchor = {0};
  ExampleX509Source trust = {candidates,CERTIFICATES,&trusted_anchor,1};
  TC_bytes encoded, crls[CRLS];
  TC_X509_time at;
  if (!read_time(argv[6],&at) || !example_read_file(argv[1],message,sizeof message,&encoded)) {
    fputs("Invalid time or unreadable/oversized SignedData file\n",stderr);
    return 2;
  }
  for (size_t i = 0; i < CERTIFICATES; ++i)
    if (!example_read_file(argv[2 + i],certificates[i],sizeof certificates[i],&candidates[i])) {
      fputs("Unreadable, empty or oversized certificate\n",stderr); return 2;
    }
  for (size_t i = 0; i < CRLS; ++i)
    if (!example_read_file(argv[4 + i],crl_bytes[i],sizeof crl_bytes[i],&crls[i])) {
      fputs("Unreadable, empty or oversized CRL\n",stderr); return 2;
    }

  const TC_TLV_limits limits = {OBJECT_BYTES,OBJECT_BYTES,ELEMENTS,EXAMPLE_CMS_FRAME_CAPACITY};
  TC_X509_workspace parser = {scratch.cms.path.validation.frames,EXAMPLE_CMS_FRAME_CAPACITY,
    scratch.cms.path.validation.oids,sizeof scratch.cms.path.validation.oids / sizeof scratch.cms.path.validation.oids[0]};
  TC_X509_certificate root;
  TC_X509_crl_index index;
  size_t work = WORK_LIMIT;
  if (TC_X509_read(candidates[0].data,candidates[0].length,&limits,&parser,&root) != TC_TLV_OK ||
      TC_X509_crl_index_init(crls,CRLS,&limits,&parser,&work,records,CRLS,&index) != TC_TLV_OK) {
    fputs("Invalid root certificate or CRL encoding\n",stderr); return 2;
  }
  /* The root file is an application-authorized trust input. */
  trusted_anchor.trust.name = root.subject;
  trusted_anchor.trust.public_key = root.public_key;
  const TC_X509_store_source source = example_x509_source(&trust);
  TC_X509_store_snapshot slot = {0};
  TC_X509_store store = {0};
  if (TC_X509_store_prepare(&slot,&source) != TC_TLV_OK ||
      TC_X509_store_publish(&store,0,&slot) != TC_TLV_OK) return 2;

  const TC_RSA_workspace rsa = {rsa_words,sizeof rsa_words / sizeof rsa_words[0]};
  const TC_X509_native_workspace native = {&ec,&rsa,TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK};
  TC_CMS_path_options options = {0};
  options.path.at = at; options.path.parsing = limits;
  options.path.max_certificates = EXAMPLE_X509_PATH_CAPACITY;
  options.path.max_input = OBJECT_BYTES;
  options.path.signatures = TC_X509_native_provider(&native);
  options.path.key_usage = TC_KEY_USAGE_DIGITAL_SIGNATURE;
  options.path.flags = TC_X509_PATH_REQUIRE_KEY_USAGE;
  options.max_candidates = EXAMPLE_CMS_CERTIFICATE_CAPACITY;
  options.max_candidate_bytes = OBJECT_BYTES;
  options.attributes = TC_CMS_ATTRIBUTES_DER;
  TC_X509_path_options crl_policy = options.path;
  crl_policy.key_usage = TC_KEY_USAGE_CRL_SIGN;
  crl_policy.flags = 0;
  const TC_CMS_revocation_policy revocation = {&index,&crl_policy,OBJECT_BYTES,
    TC_X509_CRL_COMPLETE_ONLY,TC_X509_CRL_ORDER_NUMBER};
  static const uint8_t id_data[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,1,7,1};
  const TC_CMS_validation_request request = {encoded,0,{id_data,sizeof id_data},NULL,0,{NULL,0}};
  /* This single-threaded caller owns the store and all source buffers. */
  TC_credential_status result = example_validate_cms_from_store(&request,
      &store,&options,&revocation,&work,&scratch);
  static const char* names[] = {"valid","invalid","revoked","unsupported","limit","error","unavailable"};
  printf("%s\n",names[result]);
  return result == TC_CREDENTIAL_VALID ? 0 : 1;
}
