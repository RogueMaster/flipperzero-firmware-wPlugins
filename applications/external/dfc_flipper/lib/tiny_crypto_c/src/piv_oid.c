/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/piv_oid.h>
#if TC_ENABLE_PIV_OIDS
#include <string.h>

TC_PIV_oid TC_PIV_oid_identify(TC_bytes oid, TC_PIV_oid_profile profile)
{
  /* 2.16.840.1.101.3 and 1.3.6.1.4.1.29138, respectively. */
  static const uint8_t piv_root[] = {0x60,0x86,0x48,1,0x65,3};
  static const uint8_t twic_root[] = {0x2b,6,1,4,1,0x81,0xe3,0x52};
  size_t root;
  int twic = 0;
  if (!oid.data || (profile != TC_PIV_OIDS_ONLY && profile != TC_PIV_OIDS_TWIC_COMPATIBLE))
    return TC_PIV_OID_UNKNOWN;
  if (oid.length > sizeof piv_root && !memcmp(oid.data,piv_root,sizeof piv_root))
    root = sizeof piv_root;
  else if (profile == TC_PIV_OIDS_TWIC_COMPATIBLE && oid.length > sizeof twic_root &&
      !memcmp(oid.data,twic_root,sizeof twic_root)) {
    root = sizeof twic_root;
    twic = 1;
  } else return TC_PIV_OID_UNKNOWN;
  const uint8_t* suffix = oid.data + root;
  const size_t length = oid.length - root;
  /* Certificate policies beneath .2.1.3. */
  if (length == 4 && suffix[0] == 2 && suffix[1] == 1 && suffix[2] == 3) {
    switch (suffix[3]) {
      case 5: return twic ? TC_PIV_OID_POLICY_DIGITAL_SIGNATURE : TC_PIV_OID_UNKNOWN;
      case 6: return TC_PIV_OID_POLICY_KEY_MANAGEMENT;
      case 8: return TC_PIV_OID_POLICY_DEVICES;
      case 13: return TC_PIV_OID_POLICY_AUTHENTICATION;
      case 17: return TC_PIV_OID_POLICY_CARD_AUTHENTICATION;
      /* Common Policy section 1.2; TWIC section 6 defines no paired alias. */
      case 39: return twic ? TC_PIV_OID_UNKNOWN : TC_PIV_OID_POLICY_CONTENT_SIGNING;
      default: return TC_PIV_OID_UNKNOWN;
    }
  }
  if (length == 2 && suffix[0] == 6) {
    switch (suffix[1]) {
      /* FIPS 201-3 Table B-1 content types retain their registered PIV OIDs. */
      case 1: return twic ? TC_PIV_OID_UNKNOWN : TC_PIV_OID_CHUID_CONTENT;
      case 2: return twic ? TC_PIV_OID_UNKNOWN : TC_PIV_OID_BIOMETRIC_CONTENT;
      case 5: return twic ? TC_PIV_OID_UNKNOWN : TC_PIV_OID_SIGNER_NAME;
      case 6: return TC_PIV_OID_FASCN;
      case 7: return TC_PIV_OID_CONTENT_SIGNING;
      case 8: return TC_PIV_OID_CARD_AUTHENTICATION;
      default: return TC_PIV_OID_UNKNOWN;
    }
  }
  if (length == 3 && suffix[0] == 6 && suffix[1] == 9 && suffix[2] == 1)
    return TC_PIV_OID_BACKGROUND_CHECK;
  return TC_PIV_OID_UNKNOWN;
}
#endif
