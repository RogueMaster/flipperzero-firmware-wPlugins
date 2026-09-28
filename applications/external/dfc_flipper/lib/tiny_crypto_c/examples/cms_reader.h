/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CMS_READER_H_
#define EXAMPLE_CMS_READER_H_
#include <tiny_crypto/cms.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { EXAMPLE_CMS_FRAME_CAPACITY = 16 };
typedef struct {
  TC_TLV_frame frames[EXAMPLE_CMS_FRAME_CAPACITY];
} ExampleCMSWorkspace;

enum { EXAMPLE_CMS_SIGNATURE_CAPACITY = 3072 / 8 };
typedef struct {
  ExampleCMSWorkspace parser;
  uint8_t signature[EXAMPLE_CMS_SIGNATURE_CAPACITY];
} ExampleCMSVerifyWorkspace;

/* Inspect an envelope up to 16 KiB. The result borrows input, not workspace.
 * Return parsing/limit errors to the caller; OK does not authenticate content. */
TC_TLV_result example_read_cms(TC_bytes input, size_t work_limit,
    ExampleCMSWorkspace* workspace, TC_CMS_signed_data* out);

/* Read one SignerInfo, not the surrounding signerInfos SET. Same lifetime rules. */
TC_TLV_result example_read_cms_signer(TC_bytes input, size_t work_limit,
    ExampleCMSWorkspace* workspace, TC_CMS_signer_info* out);

/* Parse every member of a signerInfos SET with one work budget. */
TC_TLV_result example_parse_cms_signers(TC_bytes encoded_set, size_t work_limit,
    ExampleCMSWorkspace* workspace);

/* Verify a parsed signer with a digest computed by the application. The key
 * must already be selected for that signer. This does not validate key trust.
 * Scratch accommodates fragmented RSA signatures through 3072 bits and ECDSA. */
TC_X509_signature_result example_verify_cms_digest(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes digest, TC_CMS_attribute_encoding encoding,
    const TC_X509_public_key* key, const TC_X509_signature_provider* provider,
    size_t work_limit, ExampleCMSVerifyWorkspace* workspace);

/* Hash raw content or a complete BER OCTET STRING, then verify the signer.
 * Same key-selection, trust and storage rules as the prehashed example. */
TC_X509_signature_result example_verify_cms_content(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes content, TC_CMS_content_encoding content_encoding,
    TC_CMS_attribute_encoding attribute_encoding, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, size_t work_limit,
    ExampleCMSVerifyWorkspace* workspace);

#ifdef __cplusplus
}
#endif
#endif
