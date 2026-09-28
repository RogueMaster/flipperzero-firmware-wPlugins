/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TINY_CRYPTO_H_
#define TINY_CRYPTO_H_

/* Include this umbrella for the configured product profile. Granular headers
 * remain available when firmware wants to minimize preprocessing work. */
#include <tiny_crypto/common.h>
#if TC_ENABLE_GZIP
#include <tiny_crypto/gzip.h>
#endif
#if TC_ENABLE_EC
#include <tiny_crypto/ec.h>
#endif
#if TC_ENABLE_RSA
#include <tiny_crypto/rsa.h>
#endif
#if TC_ENABLE_PIV_SM
#include <tiny_crypto/piv_sm.h>
#if TC_ENABLE_X509 && TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_sm_authenticate.h>
#endif
#endif

#if TC_ENABLE_TLV
#include <tiny_crypto/tlv.h>
#endif
#if TC_ENABLE_AAMVA
#include <tiny_crypto/aamva.h>
#endif
#if TC_ENABLE_TWIC_TPK || TC_ENABLE_TWIC_OBJECT_CRYPTO
#include <tiny_crypto/twic_tpk.h>
#endif
#if TC_ENABLE_DER
#include <tiny_crypto/der.h>
#include <tiny_crypto/key.h>
#endif
#if TC_ENABLE_EAC_CVC
#include <tiny_crypto/eac_cvc.h>
#endif
#if TC_ENABLE_X509
#include <tiny_crypto/x509.h>
#include <tiny_crypto/x509_crypto.h>
#endif
#if TC_ENABLE_KEY_CHALLENGE
#include <tiny_crypto/key_challenge.h>
#endif
#if TC_ENABLE_X509_PATH
#include <tiny_crypto/x509_path.h>
#include <tiny_crypto/x509_store.h>
#endif
#if TC_ENABLE_X509_REVOCATION
#include <tiny_crypto/x509_crl.h>
#include <tiny_crypto/x509_crl_source.h>
#include <tiny_crypto/x509_revocation.h>
#endif
#if TC_ENABLE_PIV_OIDS
#include <tiny_crypto/piv_oid.h>
#endif
#if TC_ENABLE_CMS
#include <tiny_crypto/cms.h>
#endif
#if TC_ENABLE_CMS_VALIDATION
#include <tiny_crypto/cms_validation.h>
#include <tiny_crypto/validation.h>
#endif
#if TC_ENABLE_FASCN
#include <tiny_crypto/fascn.h>
#endif
#if TC_ENABLE_TWIC_UUID
#include <tiny_crypto/twic_uuid.h>
#endif
#if TC_ENABLE_PIV_OBJECTS
#include <tiny_crypto/piv_biometric.h>
#include <tiny_crypto/piv_certificate.h>
#include <tiny_crypto/piv_card.h>
#include <tiny_crypto/piv_cms.h>
#include <tiny_crypto/piv_printed.h>
#include <tiny_crypto/lds.h>
#include <tiny_crypto/piv_security.h>
#endif
#if TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_cvc.h>
#endif
#if TC_ENABLE_PIV_CHUID
#include <tiny_crypto/piv_chuid.h>
#endif
#if TC_ENABLE_CREDENTIAL
#include <tiny_crypto/credential.h>
#endif
#if TC_ENABLE_TWIC_CCL
#include <tiny_crypto/twic_ccl.h>
#endif
#if TC_ENABLE_MD5
#include <tiny_crypto/md5.h>
#endif

#if TC_ENABLE_KMAC256
#include <tiny_crypto/kmac.h>
#endif

#if TC_ENABLE_AES
#include <tiny_crypto/aes.h>
#if TC_AES_ENABLE_DYNAMIC
#include <tiny_crypto/aes_dynamic.h>
#endif
#endif

#if TC_ENABLE_DES
#include <tiny_crypto/des.h>
#endif

#if TC_ENABLE_SHA1 || TC_ENABLE_SHA224 || TC_ENABLE_SHA256 || \
    TC_ENABLE_SHA384 || TC_ENABLE_SHA512
#include <tiny_crypto/hash.h>
#endif

#if TC_ENABLE_KDF
#include <tiny_crypto/kdf.h>
#endif
#if TC_ENABLE_SSKDF
#include <tiny_crypto/sskdf.h>
#endif

#endif
