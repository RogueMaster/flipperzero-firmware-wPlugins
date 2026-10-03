/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The virtual contact interface on a PIVLink for piv_vci.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_VCI_HPP_
#define TINY_CRYPTO_PIV_VCI_HPP_

#ifndef __cplusplus
#error Do not include piv_vci.hpp in a C project, include piv_vci.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_command.hpp>
#include <tiny_crypto/piv_vci.h>
#if TC_ENABLE_PIV_VCI

namespace tiny_crypto {

typedef ::TC_PIV_discovery piv_discovery;
typedef ::TC_PIV_discovery_profile piv_discovery_profile;
typedef ::TC_PIV_vci_mode piv_vci_mode;

/* TC_PIV_discovery_get. out.aid borrows response. */
TC_CPP_NODISCARD inline piv_result piv_discovery_get(PIVLink& link, piv_discovery_profile profile,
                                                     buffer response, piv_discovery& out) noexcept
{
  return ::TC_PIV_discovery_get(link.native(), profile, response, &out);
}

/* TC_PIV_vci_establish. Pass an empty pairing_code when the policy waives
 * pairing. */
TC_CPP_NODISCARD inline piv_result piv_vci_establish(PIVLink& link, const piv_discovery& discovery,
                                                     bytes pairing_code, piv_vci_mode& out) noexcept
{
  return ::TC_PIV_vci_establish(link.native(), &discovery, pairing_code, &out);
}

} // namespace tiny_crypto
#endif
#endif
