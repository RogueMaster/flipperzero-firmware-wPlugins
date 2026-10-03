/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Text output of examples/piv_inspect.c. The functions format values that
 * the library read and checked. Objects flagged TC_PIV_OBJECT_SECRET are
 * never printed. */
#ifndef EXAMPLE_PIV_INSPECT_PRINT_H_
#define EXAMPLE_PIV_INSPECT_PRINT_H_
#include <tiny_crypto/piv_card_check.h>
#include <stdio.h>

/* Short names of library results. */
const char* example_piv_result_text(TC_PIV_result result);
const char* example_piv_status_text(TC_credential_status status);

/* bytes as lower-case hex. */
void example_piv_print_hex(FILE* out, TC_bytes bytes);
/* Printable ASCII of bytes, with other bytes shown as '.'. */
void example_piv_print_text(FILE* out, TC_bytes bytes);

/* The display name of a catalog entry. */
const char* example_piv_object_name(const TC_PIV_object_info* info);

/* The selected application: AID, version, suite and card limits. */
void example_piv_print_application(FILE* out, const char* label,
                                   const TC_PIV_application* application);

/* One line per inventory entry, then the contents of the present card
 * capability, Discovery, Key History, BIT group, Security Object and printed
 * information objects. */
void example_piv_print_objects(FILE* out, const TC_PIV_inventory* inventory);

/* The card identifiers, the CHUID and the parsed card certificates of
 * report. */
void example_piv_print_certificates(FILE* out, const TC_PIV_card_report* report);

/* One line per report entry. */
void example_piv_print_report(FILE* out, const TC_PIV_card_report* report);

#endif
