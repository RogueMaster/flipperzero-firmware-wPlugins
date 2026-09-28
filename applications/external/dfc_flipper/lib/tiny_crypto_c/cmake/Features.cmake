# SPDX-License-Identifier: GPL-2.0-or-later

# Module options, dependencies, and translation units.
set(tc_module_features)

macro(tc_module_feature option macro_name description)
  tc_profile_option(${option} ${macro_name} "${description}")
  list(APPEND tc_module_features ${macro_name})
  set(tc_module_option_${macro_name} ${option})
endmacro()

tc_module_feature(TINY_CRYPTO_ENABLE_AAMVA TC_ENABLE_AAMVA
  "Build ANSI AAMVA payload readers")
set(tc_module_sources_TC_ENABLE_AAMVA src/aamva.c)

tc_module_feature(TINY_CRYPTO_ENABLE_FASCN TC_ENABLE_FASCN
  "Build FASC-N readers and writers")
set(tc_module_sources_TC_ENABLE_FASCN src/fascn.c)

tc_module_feature(TINY_CRYPTO_ENABLE_TWIC_UUID TC_ENABLE_TWIC_UUID
  "Build TWIC NEXGEN UUID helpers")
set(tc_module_requires_TC_ENABLE_TWIC_UUID TINY_CRYPTO_ENABLE_FASCN)
set(tc_module_sources_TC_ENABLE_TWIC_UUID src/twic_uuid.c)

tc_module_feature(TINY_CRYPTO_ENABLE_TWIC_TPK TC_ENABLE_TWIC_TPK
  "Build TWIC transport protection key readers")
set(tc_module_requires_TC_ENABLE_TWIC_TPK TINY_CRYPTO_ENABLE_TLV)
set(tc_module_sources_TC_ENABLE_TWIC_TPK src/twic_tpk.c)

tc_module_feature(TINY_CRYPTO_ENABLE_TWIC_OBJECT_CRYPTO
  TC_ENABLE_TWIC_OBJECT_CRYPTO "Build TWIC private-object encryption")
set(tc_module_requires_TC_ENABLE_TWIC_OBJECT_CRYPTO
  TINY_CRYPTO_ENABLE_AES TINY_CRYPTO_AES_ECB)
set(tc_module_sources_TC_ENABLE_TWIC_OBJECT_CRYPTO src/twic_cipher.c)

tc_module_feature(TINY_CRYPTO_ENABLE_PIV_OIDS TC_ENABLE_PIV_OIDS
  "Build registered PIV and TWIC identifier classification")
set(tc_module_sources_TC_ENABLE_PIV_OIDS src/piv_oid.c)

# TC_ENABLE_X509 selects the certificate reader. The layers below opt into
# path construction, revocation, and CMS independently.
tc_module_feature(TINY_CRYPTO_ENABLE_KEY_CHALLENGE TC_ENABLE_KEY_CHALLENGE
  "Build generic public-key proof-of-possession challenges")
set(tc_module_requires_TC_ENABLE_KEY_CHALLENGE TINY_CRYPTO_ENABLE_X509)
set(tc_module_sources_TC_ENABLE_KEY_CHALLENGE src/key_challenge.c)

tc_module_feature(TINY_CRYPTO_ENABLE_X509_PATH TC_ENABLE_X509_PATH
  "Build X.509 path validation and stores")
set(tc_module_requires_TC_ENABLE_X509_PATH TINY_CRYPTO_ENABLE_X509)
set(tc_module_sources_TC_ENABLE_X509_PATH
  src/x509_path.c src/x509_search.c src/x509_store.c src/x509_policy.c)

tc_module_feature(TINY_CRYPTO_ENABLE_X509_REVOCATION TC_ENABLE_X509_REVOCATION
  "Build X.509 CRL and revocation validation")
set(tc_module_requires_TC_ENABLE_X509_REVOCATION TINY_CRYPTO_ENABLE_X509_PATH)
set(tc_module_sources_TC_ENABLE_X509_REVOCATION
  src/x509_crl.c src/x509_revocation.c src/source.c src/source_der.c src/x509_crl_source.c src/x509_crl_prepare.c)

tc_module_feature(TINY_CRYPTO_ENABLE_CMS TC_ENABLE_CMS
  "Build CMS parsing and signature verification")
set(tc_module_requires_TC_ENABLE_CMS
  TINY_CRYPTO_ENABLE_X509 TINY_CRYPTO_TLV_BER TINY_CRYPTO_ENABLE_PIV_OIDS)
set(tc_module_sources_TC_ENABLE_CMS src/cms.c)

tc_module_feature(TINY_CRYPTO_ENABLE_CMS_VALIDATION TC_ENABLE_CMS_VALIDATION
  "Build CMS path and revocation validation")
set(tc_module_requires_TC_ENABLE_CMS_VALIDATION
  TINY_CRYPTO_ENABLE_CMS TINY_CRYPTO_ENABLE_X509_REVOCATION)
set(tc_module_sources_TC_ENABLE_CMS_VALIDATION
  src/cms_validation.c src/validation.c)

tc_module_feature(TINY_CRYPTO_ENABLE_PIV_OBJECTS TC_ENABLE_PIV_OBJECTS
  "Build PIV and TWIC credential-object readers")
set(tc_module_requires_TC_ENABLE_PIV_OBJECTS
  TINY_CRYPTO_ENABLE_CMS TINY_CRYPTO_ENABLE_TWIC_UUID
  TINY_CRYPTO_ENABLE_PIV_OIDS)
set(tc_module_sources_TC_ENABLE_PIV_OBJECTS
  src/piv_cms.c src/piv_biometric.c src/piv_certificate.c src/piv_card.c
  src/lds.c src/piv_security.c src/piv_printed.c)

tc_module_feature(TINY_CRYPTO_ENABLE_CREDENTIAL TC_ENABLE_CREDENTIAL
  "Build composed PIV and TWIC credential validation")
set(tc_module_requires_TC_ENABLE_CREDENTIAL
  TINY_CRYPTO_ENABLE_PIV_OBJECTS TINY_CRYPTO_ENABLE_PIV_CHUID
  TINY_CRYPTO_ENABLE_CMS_VALIDATION)
set(tc_module_sources_TC_ENABLE_CREDENTIAL src/credential.c)

function(tc_validate_module_features)
  foreach(macro_name IN LISTS tc_module_features)
    set(option_name ${tc_module_option_${macro_name}})
    if(${option_name})
      foreach(requirement IN LISTS tc_module_requires_${macro_name})
        if(NOT ${requirement})
          message(FATAL_ERROR "${option_name} requires ${requirement}")
        endif()
      endforeach()
    endif()
  endforeach()
  if(TINY_CRYPTO_ENABLE_TWIC_OBJECT_CRYPTO AND
     NOT TINY_CRYPTO_AES_KEY_BITS EQUAL 128)
    message(FATAL_ERROR
      "TINY_CRYPTO_ENABLE_TWIC_OBJECT_CRYPTO requires 128-bit AES keys")
  endif()
endfunction()

function(tc_append_module_sources output)
  set(sources ${${output}})
  foreach(macro_name IN LISTS tc_module_features)
    set(option_name ${tc_module_option_${macro_name}})
    if(${option_name})
      list(APPEND sources ${tc_module_sources_${macro_name}})
    endif()
  endforeach()
  set(${output} ${sources} PARENT_SCOPE)
endfunction()
