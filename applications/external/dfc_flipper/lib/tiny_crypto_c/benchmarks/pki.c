/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "support.h"
#include "pki_fixtures.h"

#if TC_ENABLE_X509
static const uint8_t* certificate_data;
static TC_status x509(size_t length)
{
  TC_X509_certificate certificate;
  TC_TLV_frame frames[8];
  TC_bytes oids[8];
  TC_X509_workspace workspace = {frames,8,oids,8};
  const TC_TLV_limits limits = {4096,4096,128,8};
  TC_TLV_result result = TC_X509_read(certificate_data, length, &limits, &workspace, &certificate);
  tc_benchmark_consume(&certificate);
  return result == TC_TLV_OK ? TC_OK : TC_ERROR;
}
#endif
#if TC_ENABLE_PIV_CHUID
static TC_status twic_unsigned(size_t length)
{
  TC_PIV_CHUID value;
  TC_TLV_result result = TC_PIV_CHUID_read_profile(fixture_twic_unsigned, length,
      TC_PIV_CHUID_GET_DATA, TC_CHUID_PROFILE_TWIC_UNSIGNED, &value);
  tc_benchmark_consume(&value);
  return result == TC_TLV_OK ? TC_OK : TC_ERROR;
}
static TC_status chuid(size_t length)
{
  TC_PIV_CHUID value;
  TC_TLV_result result = TC_PIV_CHUID_read(fixture_chuid, length, TC_PIV_CHUID_GET_DATA, &value);
  tc_benchmark_consume(&value);
  return result == TC_TLV_OK ? TC_OK : TC_ERROR;
}
#endif

#if TC_ENABLE_EAC_CVC
static const uint8_t* eac_data;
static int eac_inherited;
static TC_status eac(size_t length)
{
  TC_EAC_CVC value;
  TC_EAC_CVC_public_key domain;
  TC_TLV_frame frames[8];
  TC_EAC_CVC_workspace workspace = {frames,8};
  const TC_TLV_limits limits = {4096,4096,128,8};
  if (TC_EAC_CVC_read(eac_data, length, &limits, &workspace, &value) != TC_TLV_OK) return TC_ERROR;
  if (eac_inherited) {
    if (TC_EAC_CVC_public_key_read(fixture_eac_domain, sizeof fixture_eac_domain, &limits, &domain) != TC_TLV_OK ||
        TC_EAC_CVC_check_encoding(&value, &domain, &domain) != TC_TLV_OK) return TC_ERROR;
  }
  tc_benchmark_consume(&value);
  return TC_OK;
}
#endif
#if TC_ENABLE_PIV_CVC
static TC_status cvc(size_t length)
{
  TC_PIV_CVC value;
  TC_TLV_result result = TC_PIV_CVC_read(fixture_cvc, length, &value);
  tc_benchmark_consume(&value);
  return result == TC_TLV_OK ? TC_OK : TC_ERROR;
}
#endif

int main(void)
{
  tc_benchmark_profile();
#if TC_ENABLE_X509
  certificate_data = fixture_rsa;
  if (tc_benchmark_run("X.509 RSA-2048", sizeof fixture_rsa, x509)) return 1;
  certificate_data = fixture_ec;
  if (tc_benchmark_run("X.509 EC P-256", sizeof fixture_ec, x509)) return 1;
#endif
#if TC_ENABLE_PIV_CHUID
  if (tc_benchmark_run("PIV CHUID", sizeof fixture_chuid, chuid)) return 1;
  if (tc_benchmark_run("TWIC unsigned CHUID", sizeof fixture_twic_unsigned, twic_unsigned)) return 1;
#endif
#if TC_ENABLE_PIV_CVC
  if (tc_benchmark_run("PIV CVC", sizeof fixture_cvc, cvc)) return 1;
#endif
#if TC_ENABLE_EAC_CVC
  eac_data = fixture_eac_rsa; eac_inherited = 0;
  if (tc_benchmark_run("EAC RSA-2048", sizeof fixture_eac_rsa, eac)) return 1;
  eac_data = fixture_eac_ec;
  if (tc_benchmark_run("EAC explicit EC-256", sizeof fixture_eac_ec, eac)) return 1;
  eac_data = fixture_eac_inherited; eac_inherited = 1;
  if (tc_benchmark_run("EAC inherited EC with encoding checks", sizeof fixture_eac_inherited, eac)) return 1;
#endif
#if !TC_ENABLE_X509 && !TC_ENABLE_PIV_CHUID && !TC_ENABLE_PIV_CVC && !TC_ENABLE_EAC_CVC
  puts("X.509 and PIV parsing disabled");
#endif
  return 0;
}
