/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_object.h"
#include "credential_pcsc.h"
#include "credential_system.h"
#include "credential_validate.h"
#include "pki_input.h"
#include "x509_revocation.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <tiny_crypto/gzip.h>
#include <tiny_crypto/hash.h>
#include <tiny_crypto/piv_certificate.h>
#include <tiny_crypto/piv_cms.h>
#include <tiny_crypto/piv_printed.h>
#include <tiny_crypto/twic_tpk.h>
#include <tiny_crypto/x509_crl_source.h>
#include <tiny_crypto/x509_crypto.h>

enum {
  CERTIFICATE_BYTES = 8192,
  RESPONSE_BYTES = 4096,
  ISSUERS = 3,
  CRLS = 4,
  OBJECT_BYTES = 16384,
  CCL_BYTES = 16 * 1024 * 1024,
  EXCHANGES = 512,
  WORK_LIMIT = 4000000,
  MAX_RSA_BITS = 3072,
  MAX_DURATION = 30,
  CCL_READS = 32
};
enum {
  INVENTORY_BYTES = (EXAMPLE_TWIC_OBJECTS + 1) * OBJECT_BYTES +
                    EXAMPLE_CARD_RESPONSE_BYTES + EXAMPLE_CARD_STATUS_BYTES
};
enum {
  CRL_TARGETS = ISSUERS + 10,
  CRL_JOB_UNITS = 256,
  CRL_METADATA_BYTES = 4096,
  CRL_WINDOW_BYTES = 1024,
  CRL_INPUT_BYTES = 64 * 1024 * 1024,
  CRL_STEP_WORK = 60000,
  CRL_STEP_ENTRIES = 16,
  CRL_STEP_BYTES = 8192
};
static struct {
  uint8_t response[RESPONSE_BYTES], decoded[CERTIFICATE_BYTES];
  uint8_t chuid[OBJECT_BYTES];
  uint8_t fingerprints[OBJECT_BYTES], face[OBJECT_BYTES],
      stored_fingerprints[OBJECT_BYTES], stored_face[OBJECT_BYTES], tpk_hex[96];
  TC_bytes fingerprint_object, face_object;
  uint8_t printed[OBJECT_BYTES];
  TC_bytes printed_plaintext;
  uint8_t inventory_bytes[INVENTORY_BYTES];
  ExampleTWICInventory inventory;
  TC_TWIC_tpk tpk;
  /* Sequential phases share scratch; borrowed results point into input buffers.
   */
  union {
    TC_GZIP_workspace gzip;
    ExampleTWICWorkspace validation;
    ExampleCMSCredentialWorkspace object_validation;
    ExampleSecurityWorkspace security_validation;
    ExampleX509RevocationWorkspace revocation;
  } scratch;
  TC_ECDSA_workspace ec;
  TC_RSA_word rsa[TC_RSA_VERIFY_WORKSPACE_WORDS(MAX_RSA_BITS)];
  struct {
    TC_X509_crl_target targets[CRL_TARGETS];
    TC_X509_crl_storage state[CRLS][CRL_JOB_UNITS];
    TC_X509_crl_match matches[CRLS][CRL_TARGETS];
    TC_X509_crl_job *jobs[CRLS];
    uint8_t metadata[CRLS][CRL_METADATA_BYTES];
    uint8_t window[CRL_WINDOW_BYTES], entry[CRL_METADATA_BYTES],
        issuer[CRL_METADATA_BYTES];
    size_t count;
    int prepared;
  } crl;
} sensitive;
/* Provisioned public inputs stay resident and immutable during validation. */
static uint8_t root_bytes[CERTIFICATE_BYTES],
    issuer_bytes[ISSUERS][CERTIFICATE_BYTES];
static size_t issuer_lengths[ISSUERS];
static uint8_t ccl_bytes[CCL_BYTES];
static uint8_t content_certificates[ISSUERS + 1][CERTIFICATE_BYTES];
static struct {
  TC_bytes certificates[ISSUERS + 1];
  TC_X509_crl_record records[CRLS];
  TC_X509_crl_index index;
  TC_X509_store_anchor anchor;
  ExampleX509Source arrays;
  TC_X509_store_source source;
  TC_X509_store store;
  TC_X509_store_snapshot slot, *held;
  size_t max_certificates;
} content_trust;

typedef struct {
  const char *reader;
  const char *root;
  const char *root_sha256;
  const char *ccl;
  const char *issuers[ISSUERS];
  size_t issuer_count;
  const char *chuid_root;
  const char *chuid_root_sha256;
  const char *tpk_hex;
  const char *chuid_issuers[ISSUERS];
  const char *chuid_crls[CRLS];
  size_t chuid_issuer_count, chuid_crl_count;
  int chuid_ber, legacy_biometric, security_object, printed_plaintext;
  uint64_t marsec_level, max_age, minimum_publication;
  unsigned seen;
  int allow_rsa1024;
  ExampleCardRSAPadding rsa_padding;
  TC_CMS_rsa_parameters cms_rsa_parameters;
  ExampleCardReadMode read_mode;
  int piv_certificate_envelope;
} Options;

static const char usage[] =
    "Usage: twic_authenticate --reader NAME --root ROOT.der --root-sha256 HEX\n"
    "  --ccl PACKED.bin\n"
    "  [--marsec-level 1|2|3] (default: 1; CCL age uses file creation time)\n"
    "  [--minimum-publication UNIX] [--issuer ISSUER.der] [--allow-rsa1024]\n"
    "  [--rsa-padding v15|pss] (default: v15)\n"
    "  [--cms-rsa-parameters null|allow-absent] (default: null; requires "
    "--chuid-root)\n"
    "  [--chuid-root ROOT.der --chuid-root-sha256 HEX --chuid-crl ISSUER.crl "
    "[--chuid-issuer "
    "ISSUER.der] [--chuid-ber]]\n"
    "  [--tpk-hex ZTA.txt] (decrypts protected biometric objects)\n"
    "  [--legacy-biometric-signature] (permits an absent signed entryUUID)\n"
    "  [--security-object] (requires signed CHUID validation)\n"
    "  [--printed-plaintext] (requires --security-object and "
    "--tpk-hex)\n"
    "  [--extended-reads] [--piv-certificate-envelope]\n"
    "Root and CCL files must be explicitly provisioned trust inputs.\n"
    "Performs TWIC active-card authentication; leaves the PIN unchanged.";

static int number(const char *text, uint64_t *out) {
  uint64_t value = 0;
  if (!text || !*text)
    return 0;
  for (; *text; ++text) {
    if (*text < '0' || *text > '9')
      return 0;
    const unsigned digit = (unsigned)(*text - '0');
    if (value > (UINT64_MAX - digit) / 10)
      return 0;
    value = value * 10 + digit;
  }
  *out = value;
  return 1;
}

static int options_read(int argc, char **argv, Options *out) {
  enum {
    READER = 1,
    ROOT = 2,
    CCL = 4,
    MARSEC = 8,
    ROOT_DIGEST = 16,
    FLOOR = 64,
    CHUID_ROOT = 128,
    FINGERPRINTS = 256,
    RSA_PADDING = 512,
    CMS_RSA_PARAMETERS = 1024,
    CHUID_ROOT_DIGEST = 2048,
    REQUIRED = READER | ROOT | ROOT_DIGEST | CCL,
    SECONDS_PER_DAY = 24 * 60 * 60
  };
  memset(out, 0, sizeof *out);
  out->marsec_level = 1;
  for (int i = 1; i < argc; ++i) {
    const char *option = argv[i];
    if (!strcmp(option, "--printed-plaintext")) {
      if (out->printed_plaintext)
        return 0;
      out->printed_plaintext = 1;
      continue;
    }
    if (!strcmp(option, "--extended-reads")) {
      if (out->read_mode == EXAMPLE_CARD_READ_EXTENDED)
        return 0;
      out->read_mode = EXAMPLE_CARD_READ_EXTENDED;
      continue;
    }
    if (!strcmp(option, "--piv-certificate-envelope")) {
      if (out->piv_certificate_envelope)
        return 0;
      out->piv_certificate_envelope = 1;
      continue;
    }
    if (!strcmp(option, "--security-object")) {
      if (out->security_object)
        return 0;
      out->security_object = 1;
      continue;
    }
    if (!strcmp(option, "--legacy-biometric-signature")) {
      if (out->legacy_biometric)
        return 0;
      out->legacy_biometric = 1;
      continue;
    }
    if (!strcmp(option, "--chuid-ber")) {
      if (out->chuid_ber)
        return 0;
      out->chuid_ber = 1;
      continue;
    }
    if (!strcmp(option, "--allow-rsa1024")) {
      if (out->allow_rsa1024)
        return 0;
      out->allow_rsa1024 = 1;
      continue;
    }
    if (++i == argc || !*argv[i])
      return 0;
    const char *value = argv[i];
    unsigned flag;
    if (!strcmp(option, "--chuid-issuer")) {
      if (out->chuid_issuer_count == ISSUERS)
        return 0;
      out->chuid_issuers[out->chuid_issuer_count++] = value;
      continue;
    }
    if (!strcmp(option, "--chuid-crl")) {
      if (out->chuid_crl_count == CRLS)
        return 0;
      out->chuid_crls[out->chuid_crl_count++] = value;
      continue;
    }
    if (!strcmp(option, "--issuer")) {
      if (out->issuer_count == ISSUERS)
        return 0;
      out->issuers[out->issuer_count++] = value;
      continue;
    }
    if (!strcmp(option, "--reader")) {
      flag = READER;
      out->reader = value;
    } else if (!strcmp(option, "--root")) {
      flag = ROOT;
      out->root = value;
    } else if (!strcmp(option, "--root-sha256")) {
      flag = ROOT_DIGEST;
      out->root_sha256 = value;
    } else if (!strcmp(option, "--ccl")) {
      flag = CCL;
      out->ccl = value;
    } else if (!strcmp(option, "--chuid-root")) {
      flag = CHUID_ROOT;
      out->chuid_root = value;
    } else if (!strcmp(option, "--chuid-root-sha256")) {
      flag = CHUID_ROOT_DIGEST;
      out->chuid_root_sha256 = value;
    } else if (!strcmp(option, "--tpk-hex")) {
      flag = FINGERPRINTS;
      out->tpk_hex = value;
    } else if (!strcmp(option, "--cms-rsa-parameters")) {
      flag = CMS_RSA_PARAMETERS;
      if (!strcmp(value, "null"))
        out->cms_rsa_parameters = TC_CMS_RSA_PARAMETERS_NULL;
      else if (!strcmp(value, "allow-absent"))
        out->cms_rsa_parameters = TC_CMS_RSA_PARAMETERS_ALLOW_ABSENT;
      else
        return 0;
    } else if (!strcmp(option, "--rsa-padding")) {
      flag = RSA_PADDING;
      if (!strcmp(value, "v15"))
        out->rsa_padding = EXAMPLE_CARD_RSA_V15;
      else if (!strcmp(value, "pss"))
        out->rsa_padding = EXAMPLE_CARD_RSA_PSS;
      else
        return 0;
    } else if (!strcmp(option, "--marsec-level")) {
      flag = MARSEC;
      if (!number(value, &out->marsec_level) || out->marsec_level < 1 ||
          out->marsec_level > 3)
        return 0;
    } else if (!strcmp(option, "--minimum-publication")) {
      flag = FLOOR;
      if (!number(value, &out->minimum_publication))
        return 0;
    } else
      return 0;
    if (out->seen & flag)
      return 0;
    out->seen |= flag;
  }
  if (out->chuid_root ? !out->chuid_root_sha256 || !out->chuid_crl_count
                      : out->chuid_root_sha256 || out->chuid_crl_count ||
                            out->chuid_issuer_count || out->chuid_ber)
    return 0;
  if (out->tpk_hex && !out->chuid_root)
    return 0;
  if ((out->seen & CMS_RSA_PARAMETERS) && !out->chuid_root)
    return 0;
  if (out->security_object && !out->chuid_root)
    return 0;
  if (out->printed_plaintext && (!out->security_object || !out->tpk_hex))
    return 0;
  if (out->legacy_biometric && !out->tpk_hex)
    return 0;
#if !(TC_ENABLE_AES && TC_AES_ENABLE_ECB && TC_AES_KEY_BITS == 128)
  if (out->tpk_hex)
    return 0;
#endif
  out->max_age = (out->marsec_level == 1 ? 7u : 1u) * SECONDS_PER_DAY;
  return (out->seen & REQUIRED) == REQUIRED;
}

static TC_X509_workspace parser_workspace(void) {
  ExampleX509Workspace *memory =
      &sensitive.scratch.validation.certificate.validation;
  const TC_X509_workspace parser = {
      memory->frames, sizeof memory->frames / sizeof *memory->frames,
      memory->oids, sizeof memory->oids / sizeof *memory->oids};
  return parser;
}

static int hex_nibble(char value, uint8_t *out) {
  if (value >= '0' && value <= '9')
    *out = (uint8_t)(value - '0');
  else if (value >= 'a' && value <= 'f')
    *out = (uint8_t)(value - 'a' + 10);
  else if (value >= 'A' && value <= 'F')
    *out = (uint8_t)(value - 'A' + 10);
  else
    return 0;
  return 1;
}

static int root_digest_match(TC_bytes encoded, const char *expected) {
  uint8_t supplied[TC_SHA256_DIGESTLEN], actual[TC_SHA256_DIGESTLEN];
  if (!expected || strlen(expected) != sizeof supplied * 2)
    return 0;
  for (size_t i = 0; i < sizeof supplied; ++i) {
    uint8_t high, low;
    if (!hex_nibble(expected[i * 2], &high) ||
        !hex_nibble(expected[i * 2 + 1], &low))
      return 0;
    supplied[i] = (uint8_t)((high << 4) | low);
  }
  struct TC_SHA256_ctx hash;
  const int valid =
      TC_SHA256_init(&hash) == TC_OK &&
      TC_SHA256_update(&hash, encoded.data, encoded.length) == TC_OK &&
      TC_SHA256_final(&hash, actual) == TC_OK &&
      memcmp(actual, supplied, sizeof actual) == 0;
  TC_secure_zero(actual, sizeof actual);
  TC_secure_zero(supplied, sizeof supplied);
  return valid;
}

static int root_read(TC_bytes encoded, const char *expected_digest,
                     const TC_TLV_limits *limits, TC_X509_store_anchor *anchor,
                     size_t *max_certificates) {
  enum { BASIC_CONSTRAINTS = 19, KEY_USAGE = 15, NAME_CONSTRAINTS = 30 };
  TC_X509_workspace parser = parser_workspace();
  TC_X509_certificate root;
  if (!root_digest_match(encoded, expected_digest) ||
      TC_X509_read(encoded.data, encoded.length, limits, &parser, &root) !=
          TC_TLV_OK)
    return 0;
  anchor->trust = (TC_X509_trust_anchor){root.subject, root.public_key};
  TC_TLV_reader reader;
  if (TC_X509_extensions_init(&reader, root.extensions.data,
                              root.extensions.length, limits) != TC_TLV_OK)
    return 0;
  TC_X509_extension extension;
  TC_TLV_result status;
  int ca = 0;
  while ((status = TC_X509_extension_next(&reader, &extension)) == TC_TLV_OK) {
    const int standard = extension.oid.length == 3 &&
                         extension.oid.data[0] == 0x55 &&
                         extension.oid.data[1] == 0x1d;
    const unsigned id = standard ? extension.oid.data[2] : 0;
    if (id == BASIC_CONSTRAINTS) {
      TC_X509_basic_constraints constraints;
      if (TC_X509_basic_constraints_read(extension.value.data,
                                         extension.value.length,
                                         &constraints) != TC_TLV_OK ||
          !constraints.ca)
        return 0;
      ca = 1;
      if (constraints.has_path_length &&
          constraints.path_length < *max_certificates)
        *max_certificates = (size_t)constraints.path_length + 1;
    } else if (id == KEY_USAGE) {
      uint16_t usage;
      if (TC_X509_key_usage_read(extension.value.data, extension.value.length,
                                 &usage) != TC_TLV_OK ||
          !(usage & TC_KEY_USAGE_CERT_SIGN))
        return 0;
    } else if (id == NAME_CONSTRAINTS) {
      if (TC_X509_name_constraints_read(extension.value.data,
                                        extension.value.length, limits,
                                        &anchor->names) != TC_TLV_OK)
        return 0;
    } else if (extension.critical)
      return 0;
  }
  return status == TC_TLV_END && ca;
}

static int purpose_read(TC_bytes encoded, TC_PIV_oid expected,
                        const TC_TLV_limits *limits, size_t *work,
                        TC_bytes *purpose) {
  if (encoded.length > *work / 2)
    return 0;
  *work -= encoded.length * 2;
  TC_X509_workspace parser = parser_workspace();
  TC_X509_certificate certificate;
  if (TC_X509_read(encoded.data, encoded.length, limits, &parser,
                   &certificate) != TC_TLV_OK)
    return 0;
  TC_TLV_reader reader;
  if (TC_X509_extensions_init(&reader, certificate.extensions.data,
                              certificate.extensions.length,
                              limits) != TC_TLV_OK)
    return 0;
  TC_X509_extension extension;
  TC_TLV_result status;
  int found = 0;
  while ((status = TC_X509_extension_next(&reader, &extension)) == TC_TLV_OK) {
    static const uint8_t eku_oid[] = {0x55, 0x1d, 37};
    if (extension.oid.length != sizeof eku_oid ||
        memcmp(extension.oid.data, eku_oid, sizeof eku_oid))
      continue;
    TC_bytes usages[16];
    size_t count;
    if (TC_X509_extended_key_usage_read(extension.value.data,
                                        extension.value.length, usages, 16,
                                        &count) != TC_TLV_OK)
      return 0;
    for (size_t i = 0; i < count; ++i) {
      if (TC_PIV_oid_identify(usages[i], TC_PIV_OIDS_TWIC_COMPATIBLE) !=
          expected)
        continue;
      if (found)
        return 0;
      *purpose = usages[i];
      found = 1;
    }
  }
  return status == TC_TLV_END && found;
}

static int content_trust_load(const Options *options,
                              const TC_TLV_limits *limits, size_t *work) {
  memset(&content_trust, 0, sizeof content_trust);
  if (!options->chuid_root)
    return 1;
  content_trust.max_certificates = EXAMPLE_X509_PATH_CAPACITY;
  if (!example_read_file(options->chuid_root, content_certificates[0],
                         CERTIFICATE_BYTES, &content_trust.certificates[0]) ||
      !root_read(content_trust.certificates[0], options->chuid_root_sha256,
                 limits, &content_trust.anchor,
                 &content_trust.max_certificates))
    return 0;
  for (size_t i = 0; i < options->chuid_issuer_count; ++i)
    if (!example_read_file(options->chuid_issuers[i],
                           content_certificates[i + 1], CERTIFICATE_BYTES,
                           &content_trust.certificates[i + 1]))
      return 0;
  (void)work;
  content_trust.arrays = (ExampleX509Source){content_trust.certificates,
                                             options->chuid_issuer_count + 1,
                                             &content_trust.anchor, 1};
  content_trust.source = example_x509_source(&content_trust.arrays);
  return TC_X509_store_prepare(&content_trust.slot, &content_trust.source) ==
             TC_TLV_OK &&
         TC_X509_store_publish(&content_trust.store, 0, &content_trust.slot) ==
             TC_TLV_OK &&
         TC_X509_store_acquire(&content_trust.store, &content_trust.held) ==
             TC_TLV_OK;
}

static int crl_target_add(TC_bytes encoded, const TC_TLV_limits *limits,
                          size_t *work) {
  if (sensitive.crl.count == CRL_TARGETS || encoded.length > *work)
    return 0;
  *work -= encoded.length;
  TC_X509_workspace parser = parser_workspace();
  TC_X509_certificate certificate;
  if (TC_X509_read(encoded.data, encoded.length, limits, &parser,
                   &certificate) != TC_TLV_OK)
    return 0;
  sensitive.crl.targets[sensitive.crl.count++] =
      (TC_X509_crl_target){certificate.serial, certificate.issuer};
  return 1;
}

static int content_crls_prepare(const Options *options, TC_bytes encoded,
                                TC_PIV_CHUID_encoding encoding, TC_bytes card,
                                const TC_TLV_limits *limits, size_t *work) {
  if (sensitive.crl.prepared)
    return 1;
  if (!crl_target_add(card, limits, work))
    return 0;
  for (size_t i = 0; i <= options->chuid_issuer_count; ++i)
    if (!crl_target_add(content_trust.certificates[i], limits, work))
      return 0;
  for (size_t i = 0; i < options->issuer_count; ++i)
    if (!crl_target_add((TC_bytes){issuer_bytes[i], issuer_lengths[i]}, limits,
                        work))
      return 0;
  TC_PIV_CHUID chuid;
  if (TC_PIV_CHUID_read_profile(encoded.data, encoded.length, encoding,
                                TC_CHUID_PROFILE_TWIC_SIGNED,
                                &chuid) != TC_TLV_OK)
    return 0;
  TC_X509_workspace parser = parser_workspace();
  TC_CMS_signed_data cms;
  if (TC_CMS_signed_data_read(chuid.signature, limits, parser.frames,
                              parser.frame_capacity, work, &cms) != TC_TLV_OK)
    return 0;
  /* Collect candidate serials before authentication; validation checks their
   * trust. */
  if (cms.certificates.length) {
    TC_TLV_element certificates;
    TC_TLV_reader reader;
    if (TC_TLV_read(cms.certificates.data, cms.certificates.length, TC_TLV_BER,
                    limits, &certificates) != TC_TLV_OK ||
        TC_TLV_reader_init(&reader, certificates.value.data,
                           certificates.value.length, TC_TLV_BER,
                           limits) != TC_TLV_OK)
      return 0;
    TC_TLV_element certificate;
    TC_TLV_result status;
    while ((status = TC_TLV_next(&reader, &certificate)) == TC_TLV_OK)
      if (!crl_target_add(certificate.encoded, limits, work))
        return 0;
    if (status != TC_TLV_END)
      return 0;
  }
  const TC_CMS_path_workspace path =
      example_cms_path_workspace(&sensitive.scratch.object_validation.cms);
  for (size_t i = 0; i < options->chuid_crl_count; ++i) {
    FILE *file = fopen(options->chuid_crls[i], "rb");
    if (!file)
      return 0;
    TC_source source;
    if (!example_stream_source(file, CRL_INPUT_BYTES, &source) ||
        !source.length) {
      fclose(file);
      return 0;
    }
    const TC_X509_crl_prepare_options policy = {
        *limits, source.length, source.length * 3 + CRL_STEP_BYTES,
        source.length + CRL_METADATA_BYTES, source.length / 2 + 1};
    const TC_X509_crl_prepare_workspace workspace = {
        {(uint8_t *)sensitive.crl.state[i], sizeof sensitive.crl.state[i]},
        {sensitive.crl.window, sizeof sensitive.crl.window},
        {sensitive.crl.metadata[i], sizeof sensitive.crl.metadata[i]},
        {sensitive.crl.entry, sizeof sensitive.crl.entry},
        {sensitive.crl.issuer, sizeof sensitive.crl.issuer},
        parser,
        path.validation.names,
        sensitive.crl.matches[i],
        CRL_TARGETS};
    size_t budget = CRL_STEP_WORK;
    TC_TLV_result status = TC_X509_crl_prepare_begin(
        &source, sensitive.crl.targets, sensitive.crl.count, &policy,
        &workspace, &budget, &sensitive.crl.jobs[i]);
    int complete = 0;
    while (status == TC_TLV_OK && !complete) {
      budget = CRL_STEP_WORK;
      status = TC_X509_crl_prepare_step(sensitive.crl.jobs[i], CRL_STEP_ENTRIES,
                                        CRL_STEP_BYTES, &budget, &complete);
    }
    if (status == TC_TLV_OK)
      status = TC_X509_crl_prepare_finish(sensitive.crl.jobs[i],
                                          &content_trust.records[i]);
    const int closed = fclose(file) == 0;
    if (status != TC_TLV_OK || !closed)
      return 0;
  }
  content_trust.index =
      (TC_X509_crl_index){content_trust.records, options->chuid_crl_count, 0};
  sensitive.crl.prepared = 1;
  return 1;
}

#if TC_ENABLE_AES && TC_AES_ENABLE_ECB && TC_AES_KEY_BITS == 128
/* Decrypt one BC field with the loaded TPK and remove PKCS #7 padding.
 * Input and output must be separate. plaintext borrows output on success;
 * the caller wipes output during cleanup. Parsing and copying consume work. */
static int encrypted_object_decode(TC_bytes encoded, TC_buffer output,
                                   TC_bytes *plaintext, size_t *work) {
  enum { ENCRYPTED_VALUE_TAG = 0xbc };
  const TC_TLV_limits limits = {OBJECT_BYTES, OBJECT_BYTES, 4, 2};
  TC_TLV_element value;
  if (encoded.length > *work)
    return 0;
  *work -= encoded.length;
  if (TC_TLV_read(encoded.data, encoded.length, TC_TLV_ISO7816, &limits,
                  &value) != TC_TLV_OK ||
      value.encoded.length != encoded.length || value.header.tag_length != 1 ||
      value.header.tag[0] != ENCRYPTED_VALUE_TAG ||
      value.value.length > output.capacity || value.value.length > *work)
    return 0;
  *work -= value.value.length;
  memcpy(output.data, value.value.data, value.value.length);
  size_t length;
  if (TC_TWIC_object_decrypt(&sensitive.tpk, output.data, value.value.length,
                             &length) != TC_OK)
    return 0;
  *plaintext = (TC_bytes){output.data, length};
  return 1;
}
#endif

/* Read and decrypt one protected TWIC object. plaintext borrows output. */
static int protected_object_read(ExampleCardIO *io, ExampleCardReadMode mode,
                                 TC_PIV_card_profile profile, uint8_t tag_id,
                                 TC_bytes *encoded, TC_buffer stored,
                                 TC_buffer output, TC_bytes *plaintext,
                                 size_t *work) {
#if TC_ENABLE_AES && TC_AES_ENABLE_ECB && TC_AES_KEY_BITS == 128
  const TC_TLV_limits limits = {OBJECT_BYTES, OBJECT_BYTES, 4, 2};
  if (!encoded->data) {
    ExampleCardResponse response;
    ExampleCardModel model;
    if (example_card_select(io, EXAMPLE_CARD_TWIC, stored.data, stored.capacity,
                            &response) != EXAMPLE_CARD_OK ||
        example_card_identity((TC_bytes){stored.data, response.length},
                              EXAMPLE_CARD_TWIC, &model) != TC_TLV_OK ||
        model != (profile == TC_TWIC_LEGACY_CARD
                      ? EXAMPLE_CARD_MODEL_TWIC_LEGACY
                      : EXAMPLE_CARD_MODEL_TWIC_NEXGEN))
      return 0;
    const uint8_t tag[] = {0xdf, 0xc1, tag_id};
    if (example_card_object_read(io, mode, tag, sizeof tag, stored.data,
                                 stored.capacity, &response) != EXAMPLE_CARD_OK)
      return 0;
    TC_TLV_element outer;
    if (response.length > *work / 2)
      return 0;
    *work -= response.length * 2;
    if (TC_TLV_read(stored.data, response.length, TC_TLV_ISO7816, &limits,
                    &outer) != TC_TLV_OK ||
        outer.encoded.length != response.length ||
        outer.encoded.data[0] != 0x53)
      return 0;
    /* Security-object hashes cover the stored BC field, including ciphertext.
     */
    *encoded = outer.value;
  }
  return encrypted_object_decode(*encoded, output, plaintext, work);
#else
  (void)io;
  (void)mode;
  (void)profile;
  (void)tag_id;
  (void)encoded;
  (void)stored;
  (void)output;
  (void)plaintext;
  (void)work;
  return 0;
#endif
}

static int signed_objects_check(ExampleCardIO *io, const Options *options,
                                TC_bytes certificate,
                                TC_PIV_card_profile profile,
                                const TC_X509_path_options *card_policy,
                                TC_bytes *encoded, TC_bytes *fingerprints,
                                TC_bytes *face, size_t *work) {
  if (!options->chuid_root)
    return 1;
  ExampleCardIdentity card;
  if (example_read_card_identity(
          certificate, profile, &card_policy->parsing,
          &sensitive.scratch.validation.certificate.validation, work,
          &card) != TC_TLV_OK)
    return 0;
  /* Keep the certificate in its original buffer while reading the CHUID. */
  if (!encoded->data) {
    if (options->security_object) {
      const ExampleCardModel model = profile == TC_TWIC_LEGACY_CARD
                                         ? EXAMPLE_CARD_MODEL_TWIC_LEGACY
                                         : EXAMPLE_CARD_MODEL_TWIC_NEXGEN;
      if (example_twic_inventory_read(
              io, model, options->read_mode, sensitive.inventory_bytes,
              sizeof sensitive.inventory_bytes, OBJECT_BYTES, work,
              &sensitive.inventory) != EXAMPLE_CARD_OK)
        return 0;
      for (size_t i = 0; i < sensitive.inventory.count; ++i) {
        if (sensitive.inventory.objects[i].container == EXAMPLE_TWIC_CHUID)
          *encoded = sensitive.inventory.objects[i].contents;
        if (sensitive.inventory.objects[i].container ==
            EXAMPLE_TWIC_FINGERPRINTS)
          sensitive.fingerprint_object =
              sensitive.inventory.objects[i].contents;
        if (sensitive.inventory.objects[i].container == EXAMPLE_TWIC_FACE)
          sensitive.face_object = sensitive.inventory.objects[i].contents;
      }
      if (!encoded->data || !sensitive.fingerprint_object.data ||
          (profile == TC_TWIC_NEXGEN_CARD && !sensitive.face_object.data))
        return 0;
    } else {
      static const uint8_t tag[] = {0x5f, 0xc1, 2};
      ExampleCardResponse response;
      if (example_card_object_read(io, options->read_mode, tag, sizeof tag,
                                   sensitive.chuid, sizeof sensitive.chuid,
                                   &response) != EXAMPLE_CARD_OK)
        return 0;
      *encoded = (TC_bytes){sensitive.chuid, response.length};
    }
  }
  const TC_PIV_CHUID_encoding encoding =
      options->security_object ? TC_PIV_CHUID_CONTENTS : TC_PIV_CHUID_CONTAINER;
  const TC_TLV_limits limits = {OBJECT_BYTES, OBJECT_BYTES, 1024,
                                EXAMPLE_CMS_FRAME_CAPACITY};
  if (!content_crls_prepare(options, *encoded, encoding, certificate, &limits,
                            work))
    return 0;
  TC_validation_options validation = {0};
  validation.at = card_policy->at;
  validation.signatures = card_policy->signatures;
  validation.parsing = limits;
  validation.max_certificates = content_trust.max_certificates;
  validation.max_input = OBJECT_BYTES * EXAMPLE_X509_PATH_CAPACITY;
  validation.max_candidates = EXAMPLE_CMS_CERTIFICATE_CAPACITY;
  validation.max_candidate_bytes =
      OBJECT_BYTES * EXAMPLE_CMS_CERTIFICATE_CAPACITY;
  validation.attributes = options->chuid_ber
                              ? TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER
                              : TC_CMS_ATTRIBUTES_DER;
  validation.rsa_parameters = options->cms_rsa_parameters;
  validation.certificate = (TC_validation_certificate_policy){
      card_policy->initial_policies, card_policy->initial_policy_count,
      card_policy->anchor_names,     {NULL, 0},
      card_policy->key_usage,        card_policy->flags};
  validation.crl_signer.key_usage = TC_KEY_USAGE_CRL_SIGN;
  const TC_validation_trust trust = {&content_trust.held->source,
                                     &content_trust.index};
  const TC_PIV_CHUID_validation_request request = {*encoded,
                                                   encoding,
                                                   profile,
                                                   TC_CHUID_PROFILE_TWIC_SIGNED,
                                                   0,
                                                   &card.identifiers,
                                                   &card.expiration};
  TC_CMS_path_workspace chuid_path =
      example_cms_path_workspace(&sensitive.scratch.object_validation.cms);
  const TC_CMS_credential_workspace chuid_workspace =
      example_cms_credential_workspace(&sensitive.scratch.object_validation,
                                       &chuid_path);
  TC_validation_context chuid_context;
  TC_PIV_CHUID_result accepted;
  if (TC_validation_context_init(&trust, &validation, &chuid_workspace,
                                 &chuid_context) != TC_RESULT_OK ||
      TC_PIV_CHUID_validate(&request, &chuid_context, work, &accepted) !=
          TC_CREDENTIAL_VALID)
    return 0;
  if (options->security_object) {
    TC_PIV_security_data entries[EXAMPLE_TWIC_OBJECTS];
    int printed_found = 0;
    TC_bytes unsigned_chuid = {NULL, 0};
    for (size_t i = 0; i < sensitive.inventory.count; ++i) {
      entries[i] =
          (TC_PIV_security_data){sensitive.inventory.objects[i].container,
                                 &sensitive.inventory.objects[i].contents, 1};
      if (options->printed_plaintext &&
          entries[i].container == EXAMPLE_TWIC_PRINTED) {
        printed_found = 1;
#if TC_ENABLE_AES && TC_AES_ENABLE_ECB && TC_AES_KEY_BITS == 128
        /* Both validation passes hash the same retained plaintext. */
        if (!sensitive.printed_plaintext.data &&
            !encrypted_object_decode(
                sensitive.inventory.objects[i].contents,
                (TC_buffer){sensitive.printed, sizeof sensitive.printed},
                &sensitive.printed_plaintext, work))
          return 0;
        entries[i].parts = &sensitive.printed_plaintext;
#else
        return 0;
#endif
      }
      if (entries[i].container == TC_TWIC_UNSIGNED_CHUID_CONTAINER)
        unsigned_chuid = sensitive.inventory.objects[i].contents;
    }
    if (options->printed_plaintext && !printed_found)
      return 0;
    const TC_PIV_security_validation_request security = {
        sensitive.inventory.security,
        TC_PIV_SECURITY_CONTAINER,
        profile,
        accepted.signer,
        &card.expiration,
        entries,
        sensitive.inventory.count};
    TC_CMS_path_workspace security_path = example_cms_path_workspace(
        &sensitive.scratch.security_validation.credential.cms);
    const TC_CMS_credential_workspace security_credential =
        example_cms_credential_workspace(
            &sensitive.scratch.security_validation.credential, &security_path);
    TC_validation_context security_context;
    const TC_PIV_security_validation_workspace security_workspace = {
        sensitive.scratch.security_validation.content,
        sizeof sensitive.scratch.security_validation.content};
    const TC_TWIC_unsigned_CHUID_validation_request unsigned_request = {
        unsigned_chuid, TC_PIV_CHUID_CONTENTS, profile, &card.identifiers};
    TC_PIV_security_result accepted_security;
    if (TC_validation_context_init(&trust, &validation, &security_credential,
                                   &security_context) != TC_RESULT_OK ||
        TC_PIV_security_validate(&security, &security_context,
                                 &security_workspace, work,
                                 &accepted_security) != TC_CREDENTIAL_VALID ||
        TC_TWIC_unsigned_CHUID_validate(&unsigned_request, &accepted_security,
                                        &security_context,
                                        work) != TC_CREDENTIAL_VALID)
      return 0;
    if (options->printed_plaintext) {
      TC_PIV_printed printed;
      int current;
      if (TC_PIV_printed_read(
              sensitive.printed_plaintext, TC_PIV_PRINTED_CONTENTS,
              TC_PIV_PRINTED_PROFILE_TWIC, &printed) != TC_TLV_OK ||
          TC_PIV_printed_expiration_check(&printed, accepted.object.expiration,
                                          &validation.at,
                                          &current) != TC_TLV_OK ||
          !current)
        return 0;
    }
  }
  if (!options->tpk_hex)
    return 1;
  if (!fingerprints->data &&
      !protected_object_read(
          io, options->read_mode, profile, 3, &sensitive.fingerprint_object,
          (TC_buffer){sensitive.stored_fingerprints,
                      sizeof sensitive.stored_fingerprints},
          (TC_buffer){sensitive.fingerprints, sizeof sensitive.fingerprints},
          fingerprints, work))
    return 0;
  if (profile == TC_TWIC_NEXGEN_CARD && !face->data &&
      !protected_object_read(
          io, options->read_mode, profile, 8, &sensitive.face_object,
          (TC_buffer){sensitive.stored_face, sizeof sensitive.stored_face},
          (TC_buffer){sensitive.face, sizeof sensitive.face}, face, work))
    return 0;
  const TC_PIV_CMS_kind signature_profile = options->legacy_biometric
                                                ? TC_PIV_CMS_BIOMETRIC_LEGACY
                                                : TC_PIV_CMS_BIOMETRIC;
  const TC_bytes objects[] = {*fingerprints, *face};
  const TC_PIV_CBEFF_format formats[] = {TC_PIV_CBEFF_FINGERPRINT_TEMPLATE,
                                         TC_PIV_CBEFF_FACE_IMAGE};
  const size_t object_count =
      profile == TC_TWIC_NEXGEN_CARD ? sizeof objects / sizeof *objects : 1;
  for (size_t i = 0; i < object_count; ++i) {
    const TC_PIV_biometric_validation_request biometric = {
        objects[i],
        profile,
        accepted.object.fascn,
        accepted.object.card_uuid,
        accepted.signer,
        &card.expiration,
        signature_profile,
        formats[i],
        1};
    if (TC_PIV_biometric_validate(&biometric, &chuid_context, work) !=
        TC_CREDENTIAL_VALID)
      return 0;
  }
  return 1;
}

static int card_certificate(ExampleCardIO *io, const Options *options,
                            TC_PIV_card_profile *profile, TC_bytes *encoded,
                            size_t *work) {
  ExampleCardResponse response;
  if (example_card_select(io, EXAMPLE_CARD_TWIC, sensitive.response,
                          sizeof sensitive.response,
                          &response) != EXAMPLE_CARD_OK)
    return 0;
  ExampleCardModel model;
  if (example_card_identity((TC_bytes){sensitive.response, response.length},
                            EXAMPLE_CARD_TWIC, &model) != TC_TLV_OK)
    return 0;
  TC_PIV_certificate_profile container_profile = TC_PIV_CERTIFICATE_TWIC;
  if (model == EXAMPLE_CARD_MODEL_TWIC_LEGACY) {
    *profile = TC_TWIC_LEGACY_CARD;
    if (example_card_select(io, EXAMPLE_CARD_PIV, sensitive.response,
                            sizeof sensitive.response,
                            &response) != EXAMPLE_CARD_OK ||
        example_card_identity((TC_bytes){sensitive.response, response.length},
                              EXAMPLE_CARD_PIV, &model) != TC_TLV_OK)
      return 0;
    container_profile = TC_PIV_CERTIFICATE_SLOT;
  } else if (model == EXAMPLE_CARD_MODEL_TWIC_NEXGEN)
    *profile = TC_TWIC_NEXGEN_CARD;
  else
    return 0;
  if (options->piv_certificate_envelope)
    container_profile = TC_PIV_CERTIFICATE_SLOT;
  static const uint8_t tag[] = {0x5f, 0xc1, 1};
  if (example_card_object_read(io, options->read_mode, tag, sizeof tag,
                               sensitive.response, sizeof sensitive.response,
                               &response) != EXAMPLE_CARD_OK)
    return 0;
  TC_PIV_certificate container;
  if (TC_PIV_certificate_read((TC_bytes){sensitive.response, response.length},
                              container_profile, &container) != TC_TLV_OK)
    return 0;
  *encoded = container.certificate;
  if (container.compression == TC_PIV_CERTIFICATE_GZIP) {
    size_t decoded;
    if (TC_GZIP_decode(encoded->data, encoded->length, sensitive.decoded,
                       sizeof sensitive.decoded, &sensitive.scratch.gzip, work,
                       &decoded) != TC_GZIP_OK)
      return 0;
    *encoded = (TC_bytes){sensitive.decoded, decoded};
  }
  return 1;
}

int main(int argc, char **argv) {
  if (argc == 2 && !strcmp(argv[1], "--help")) {
    puts(usage);
    return 0;
  }
  Options options;
  if (!options_read(argc, argv, &options)) {
    fputs(usage, stderr);
    return 2;
  }
#if TC_ENABLE_AES && TC_AES_SBOX_MODE == TC_AES_SBOX_MODE_RUNTIME
  TC_AES_init_sbox();
#endif
  const struct rlimit core_limit = {0, 0};
  if (setrlimit(RLIMIT_CORE, &core_limit) ||
      mlock(&sensitive, sizeof sensitive)) {
    fputs("Unable to protect credential memory\n", stderr);
    return 1;
  }
  int accepted = 0;
  const char *failure = "Unable to load provisioned root or CCL input";
  ExampleCardPCSC connection = {0};
  TC_TWIC_CCL_snapshot slot = {0}, *held = NULL;
  TC_TWIC_CCL_store ccl_store = {0};
  TC_bytes root, candidates[ISSUERS + 1], image;
  TC_X509_store_anchor anchor = {0};
  const TC_TLV_limits limits = {CERTIFICATE_BYTES, CERTIFICATE_BYTES, 512, 16};
  size_t max_certificates = EXAMPLE_X509_PATH_CAPACITY, work = WORK_LIMIT;
  const TC_TLV_limits content_limits = {OBJECT_BYTES, OBJECT_BYTES, 1024,
                                        EXAMPLE_CMS_FRAME_CAPACITY};
  if (options.tpk_hex) {
    TC_bytes encoded_key;
    failure = "Unable to read the supplied privacy key";
    if (!example_read_file(options.tpk_hex, sensitive.tpk_hex,
                           sizeof sensitive.tpk_hex, &encoded_key))
      goto cleanup;
    while (encoded_key.length &&
           (encoded_key.data[encoded_key.length - 1] == '\n' ||
            encoded_key.data[encoded_key.length - 1] == '\r'))
      --encoded_key.length;
    if (TC_TWIC_tpk_read(encoded_key, TC_TWIC_TPK_BARCODE_HEX,
                         &sensitive.tpk) != TC_TLV_OK)
      goto cleanup;
    TC_secure_zero(sensitive.tpk_hex, sizeof sensitive.tpk_hex);
  }
  if (!content_trust_load(&options, &content_limits, &work)) {
    failure = "Unable to load content-signer trust or revocation inputs";
    goto cleanup;
  }
  uint64_t ccl_created;
  if (!example_read_file(options.root, root_bytes, sizeof root_bytes, &root) ||
      !root_read(root, options.root_sha256, &limits, &anchor,
                 &max_certificates) ||
      !example_read_created_file(options.ccl, ccl_bytes, sizeof ccl_bytes,
                                 &image, &ccl_created))
    goto cleanup;
  for (size_t i = 0; i < options.issuer_count; ++i) {
    if (!example_read_file(options.issuers[i], issuer_bytes[i],
                           sizeof issuer_bytes[i], &candidates[i])) {
      failure = "Unable to load issuer candidate";
      goto cleanup;
    }
    issuer_lengths[i] = candidates[i].length;
  }
  TC_TWIC_CCL_index index;
  const TC_TWIC_CCL_metadata metadata = {ccl_created, ccl_created};
  failure = "Invalid or oversized packed CCL image";
  if (TC_TWIC_CCL_index_from_memory(&image, CCL_BYTES / TC_TWIC_CCL_FASCN_BYTES,
                                    &index) != TC_TWIC_CCL_OK ||
      TC_TWIC_CCL_store_prepare(&slot, &index, &metadata) != TC_TWIC_CCL_OK ||
      TC_TWIC_CCL_store_publish(&ccl_store, 0, &slot) != TC_TWIC_CCL_OK ||
      TC_TWIC_CCL_store_acquire(&ccl_store, &held) != TC_TWIC_CCL_OK)
    goto cleanup;
  const TC_RSA_workspace rsa = {sensitive.rsa,
                                sizeof sensitive.rsa / sizeof *sensitive.rsa};
  const TC_X509_native_workspace native = {
      &sensitive.ec, &rsa, TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK};
  TC_X509_path_options path = {0};
  failure = "Unable to obtain a valid current time";
  if (!example_card_now(&path.at))
    goto cleanup;
  int64_t started;
  if (TC_X509_time_to_unix(&path.at, &started) != TC_TLV_OK || started < 0)
    goto cleanup;
  TC_TWIC_CCL_freshness_policy freshness = {(uint64_t)started, options.max_age,
                                            options.minimum_publication};
  failure = "CCL publication metadata is stale or invalid";
  if (TC_TWIC_CCL_check_freshness(&metadata, &freshness) != TC_TWIC_CCL_OK)
    goto cleanup;
  path.parsing = limits;
  path.max_certificates = max_certificates;
  path.max_input = CERTIFICATE_BYTES * EXAMPLE_X509_PATH_CAPACITY;
  path.max_work = work;
  path.signatures = TC_X509_native_provider(&native);
  path.key_usage = TC_KEY_USAGE_DIGITAL_SIGNATURE;
  path.flags = TC_X509_PATH_REQUIRE_KEY_USAGE |
               TC_X509_PATH_REQUIRE_EXTENDED_KEY_USAGE |
               TC_X509_PATH_INHIBIT_ANY_PURPOSE;
  static const uint8_t any_oid[] = {0x55, 0x1d, 0x20, 0};
  const TC_bytes any_policy = {any_oid, sizeof any_oid};
  path.initial_policies = &any_policy;
  path.initial_policy_count = 1;
  /* The root certificate can also sign a CRL; its anchor remains explicit. */
  candidates[options.issuer_count] = root;
  ExampleX509Source arrays = {candidates, options.issuer_count + 1, &anchor, 1};
  const TC_X509_store_source trust = example_x509_source(&arrays);
  failure = "Unable to acquire the reader transaction";
  if (!example_card_pcsc_open(&connection, options.reader))
    goto cleanup;
  ExampleCardIO io = {example_card_pcsc_transmit, &connection, EXCHANGES, 0};
  TC_PIV_card_profile profile;
  TC_bytes certificate;
  failure = "Unable to read a supported card-authentication certificate";
  if (!card_certificate(&io, &options, &profile, &certificate, &work) ||
      !purpose_read(certificate, TC_PIV_OID_CARD_AUTHENTICATION, &limits, &work,
                    &path.purpose))
    goto cleanup;
  const ExampleTWICRequest request = {certificate,
                                      &trust,
                                      &path,
                                      profile,
                                      options.allow_rsa1024,
                                      held,
                                      options.max_age,
                                      options.minimum_publication,
                                      CCL_READS,
                                      options.rsa_padding};
  const ExampleTWICResult result =
      example_twic_authenticate(&io, &request, example_card_random, NULL,
                                &sensitive.scratch.validation, &work);
  static const char *results[] = {"Authenticated",
                                  "Certificate or card proof invalid",
                                  "Credential cancelled",
                                  "CCL superseded or stale",
                                  "CCL unavailable",
                                  "Unsupported credential algorithm or profile",
                                  "Validation resource limit reached",
                                  "Card transport failed",
                                  "Validation API or provider failure"};
  failure = (unsigned)result < sizeof results / sizeof *results
                ? results[result]
                : "Unexpected validation result";
  if (result != EXAMPLE_TWIC_AUTHENTICATED)
    goto cleanup;
  TC_bytes encoded_chuid = {NULL, 0};
  TC_bytes fingerprints = {NULL, 0};
  TC_bytes face = {NULL, 0};
  failure = "Signed credential object validation failed";
  if (!signed_objects_check(&io, &options, certificate, profile, &path,
                            &encoded_chuid, &fingerprints, &face, &work))
    goto cleanup;
  /* Recheck time-sensitive decisions after the physical card exchange. */
  failure = "Clock failure, rollback or transaction time limit";
  if (!example_card_now(&path.at))
    goto cleanup;
  int64_t finished;
  if (TC_X509_time_to_unix(&path.at, &finished) != TC_TLV_OK ||
      finished < started || finished - started > MAX_DURATION)
    goto cleanup;
  freshness.now = (uint64_t)finished;
  failure = "CCL freshness limit reached during the transaction";
  if (TC_TWIC_CCL_check_freshness(&metadata, &freshness) != TC_TWIC_CCL_OK)
    goto cleanup;
  ExampleCardIdentity final_identity;
  failure = "Unable to recheck the card cancellation identifier";
  if (example_read_card_identity(
          certificate, profile, &path.parsing,
          &sensitive.scratch.validation.certificate.validation, &work,
          &final_identity) != TC_TLV_OK ||
      example_twic_cancellation_check(held, &freshness, CCL_READS,
                                      final_identity.identifiers.fascn) !=
          EXAMPLE_TWIC_AUTHENTICATED)
    goto cleanup;
  path.max_work = work;
  const TC_X509_path_workspace validation = example_x509_workspace(
      &sensitive.scratch.validation.certificate.validation);
  const TC_X509_search_workspace search =
      example_x509_search_workspace(&sensitive.scratch.validation.certificate);
  TC_X509_search_result final_path;
  failure = "Certificate path failed its final time check";
  accepted = TC_X509_path_build(certificate, &trust, &path, &validation,
                                &search, &final_path) == TC_X509_PATH_VALID;
  if (accepted) {
    if (final_path.validation.work_used > work) {
      accepted = 0;
      failure = "Final path work limit";
    } else
      work -= final_path.validation.work_used;
  }
  if (accepted && sensitive.crl.prepared) {
    TC_bytes chain[EXAMPLE_X509_PATH_CAPACITY];
    if (final_path.count > EXAMPLE_X509_PATH_CAPACITY) {
      accepted = 0;
      failure = "Card revocation path limit";
    } else {
      memcpy(chain, final_path.path, final_path.count * sizeof *chain);
      TC_X509_path_options signer_policy = path;
      signer_policy.purpose = (TC_bytes){NULL, 0};
      signer_policy.key_usage = TC_KEY_USAGE_CRL_SIGN;
      signer_policy.flags = TC_X509_PATH_REQUIRE_KEY_USAGE;
      const TC_X509_revocation_options revocation = {&content_trust.index,
                                                     &trust,
                                                     &signer_policy,
                                                     final_path.anchor_index,
                                                     CERTIFICATE_BYTES *
                                                         (ISSUERS + 1),
                                                     TC_X509_CRL_COMPLETE_ONLY,
                                                     TC_X509_CRL_ORDER_NUMBER};
      TC_X509_revocation_result result;
      failure = "Card certificate revocation check failed";
      const TC_TLV_result checked = example_check_path_revocation(
          chain, final_path.count, &revocation, &work,
          &sensitive.scratch.revocation, &result);
      accepted = checked == TC_TLV_OK && result.status == TC_X509_CRL_UNREVOKED;
    }
  }
  if (accepted &&
      !signed_objects_check(&io, &options, certificate, profile, &path,
                            &encoded_chuid, &fingerprints, &face, &work)) {
    accepted = 0;
    failure = "Signed credential object failed its final time check";
  }
cleanup:
  for (size_t i = 0; i < CRLS; ++i)
    if (sensitive.crl.jobs[i])
      TC_X509_crl_prepare_clear(sensitive.crl.jobs[i]);
  if (content_trust.held &&
      TC_X509_store_release(content_trust.held) != TC_TLV_OK) {
    accepted = 0;
    failure = "Content-signer trust release failed";
  }
  if (held && TC_TWIC_CCL_store_release(held) != TC_TWIC_CCL_OK) {
    accepted = 0;
    failure = "CCL release failed";
  }
  if (!example_card_pcsc_close(&connection)) {
    accepted = 0;
    failure = "Reader cleanup failed";
  }
  TC_secure_zero(&sensitive, sizeof sensitive);
  if (munlock(&sensitive, sizeof sensitive)) {
    accepted = 0;
    failure = "Credential memory cleanup failed";
  }
  if (!accepted)
    fprintf(stderr, "%s\n", failure);
  if (accepted && options.chuid_root)
    puts("Signed CHUID authentication succeeded");
  if (accepted && options.tpk_hex)
    puts("Encrypted biometric object authentication succeeded");
  if (accepted && options.security_object)
    puts("Security object and stored-data hashes verified");
  puts(accepted ? "TWIC active-card authentication succeeded"
                : "TWIC active-card authentication failed");
  return accepted ? 0 : 1;
}
