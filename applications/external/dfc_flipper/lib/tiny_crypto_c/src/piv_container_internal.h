/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PIV_CONTAINER_INTERNAL_H
#define TC_PIV_CONTAINER_INTERNAL_H

#include <tiny_crypto/tlv.h>

/* Return a borrowed view of the complete 0x53 container's contents. */
TC_TLV_result tc_piv_container_contents(TC_bytes encoded, const TC_TLV_limits* limits,
                                        TC_bytes* contents);

#endif
