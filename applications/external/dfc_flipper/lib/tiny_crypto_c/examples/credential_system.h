/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_SYSTEM_H_
#define EXAMPLE_CREDENTIAL_SYSTEM_H_
#include <tiny_crypto/x509.h>
/* POSIX host services. Clock output is UTC; entropy comes from the OS CSPRNG. */
int example_card_now(TC_X509_time* out);
TC_status example_card_random(void* context, uint8_t* out, size_t length);
#endif
