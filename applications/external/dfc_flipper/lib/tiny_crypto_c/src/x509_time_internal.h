/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_TIME_INTERNAL_H_
#define TC_X509_TIME_INTERNAL_H_
#include <tiny_crypto/x509.h>
/* A framed UTC/GeneralizedTime element; calendar output changes only on OK. */
TC_TLV_result tc_x509_time_value(const TC_TLV_element* element, TC_X509_time* out);
#endif
