/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Certificate container decoding to DER. */
#include <tiny_crypto/piv_certificate.h>
#if TC_ENABLE_PIV_OBJECTS && TC_ENABLE_GZIP
#include "internal.h"
#include "pki_internal.h"

static TC_TLV_result gzip_result(TC_GZIP_result result)
{
  switch (result) {
  case TC_GZIP_OK:
    return TC_TLV_OK;
  case TC_GZIP_LIMIT:
    return TC_TLV_LIMIT;
  case TC_GZIP_UNSUPPORTED:
    return TC_TLV_UNSUPPORTED;
  case TC_GZIP_ARGUMENT:
    return TC_TLV_ARGUMENT;
  case TC_GZIP_INVALID:
  default:
    return TC_TLV_INVALID;
  }
}

/* The certificate is exactly one DER SEQUENCE (RFC 5280 section 4.1). */
static int der_sequence(TC_bytes certificate)
{
  static const TC_TLV_limits limits = {SIZE_MAX, SIZE_MAX, 1, 1};
  TC_TLV_element element;
  return TC_TLV_read(certificate, TC_TLV_DER, &limits, &element) == TC_TLV_OK &&
         tc_pki_tag(&element, 0x30) && element.header.constructed &&
         element.encoded.length == certificate.length;
}

static TC_TLV_result decode(TC_bytes container, TC_PIV_certificate_profile profile,
                            size_t max_certificate_bytes, TC_GZIP_workspace* gzip, size_t* work,
                            TC_buffer der, TC_PIV_certificate* out)
{
  TC_PIV_certificate parsed;
  TC_TLV_result result =
      TC_PIV_certificate_read(container, profile, max_certificate_bytes, &parsed);
  if (result != TC_TLV_OK)
    return result;
  if (parsed.compression == TC_PIV_CERTIFICATE_GZIP) {
    size_t length = 0;
    /* SP 800-73-5 Part 1 Appendix A: CertInfo 01 marks GZIP compression
     * (RFC 1952). */
    result = gzip_result(TC_GZIP_decode(parsed.certificate, gzip, work, der, &length));
    if (result != TC_TLV_OK)
      return result;
    parsed.certificate = (TC_bytes){der.data, length};
  }
  if (!der_sequence(parsed.certificate))
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_certificate_decode(TC_bytes container, TC_PIV_certificate_profile profile,
                                        size_t max_certificate_bytes, TC_GZIP_workspace* gzip,
                                        size_t* work, TC_buffer der, TC_PIV_certificate* out)
{
  if (!gzip || !work || !out || !tc_internal_span_valid(container.data, container.length) ||
      !tc_internal_span_valid(der.data, der.capacity) || !max_certificate_bytes ||
      (profile != TC_PIV_CERTIFICATE_SLOT && profile != TC_PIV_CERTIFICATE_TWIC &&
       profile != TC_PIV_CERTIFICATE_SM_SIGNER))
    return TC_TLV_ARGUMENT;
  /* container, gzip, work, der and out are pairwise disjoint. */
  const struct {
    const void* data;
    size_t length;
  } regions[] = {{container.data, container.length},
                 {gzip, sizeof *gzip},
                 {work, sizeof *work},
                 {der.data, der.capacity},
                 {out, sizeof *out}};
  const size_t count = sizeof regions / sizeof *regions;
  for (size_t i = 0; i < count; ++i)
    for (size_t j = i + 1; j < count; ++j)
      if (!tc_internal_ranges_disjoint(regions[i].data, regions[i].length, regions[j].data,
                                       regions[j].length))
        return TC_TLV_ARGUMENT;
  const TC_TLV_result result =
      decode(container, profile, max_certificate_bytes, gzip, work, der, out);
  if (result != TC_TLV_OK)
    TC_secure_zero(der.data, der.capacity);
  return result;
}
#endif
