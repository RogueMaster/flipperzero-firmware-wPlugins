/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_inspect_print.h"

static const char* kind_text(uint8_t kind)
{
  static const char* const names[] = {"mandatory object",
                                      "certificate path",
                                      "revocation",
                                      "certificate identifiers",
                                      "CHUID",
                                      "security signature",
                                      "security digest",
                                      "biometric",
                                      "printed expiration",
                                      "discovery consistency",
                                      "SM signer",
                                      "SM CVC",
                                      "key proof",
                                      "copy match"};
  return kind < sizeof names / sizeof *names ? names[kind] : "unknown";
}

static const char* outcome_text(uint8_t outcome)
{
  static const char* const names[] = {"PASSED", "FAILED", "NOT_CHECKABLE"};
  return outcome < sizeof names / sizeof *names ? names[outcome] : "unknown";
}

static const char* reason_text(uint8_t reason)
{
  static const char* const names[] = {"",
                                      "absent",
                                      "empty",
                                      "restricted",
                                      "denied",
                                      "no evidence",
                                      "unsupported",
                                      "dependency",
                                      "not requested",
                                      "limit",
                                      "card status",
                                      "oversized"};
  return reason < sizeof names / sizeof *names ? names[reason] : "unknown";
}

void example_piv_print_report(FILE* out, const TC_PIV_card_report* report)
{
  size_t outcomes[3] = {0, 0, 0};
  fprintf(out, "Checks (%zu):\n", report->count);
  for (size_t i = 0; i < report->count; ++i) {
    const TC_PIV_check* check = &report->checks[i];
    fprintf(out, "  %-13s %-23s", outcome_text(check->outcome), kind_text(check->kind));
    if (check->container)
      fprintf(out, " %04x", check->container);
    else
      fputs("     ", out);
    if (check->key_reference)
      fprintf(out, " key %02x", check->key_reference);
    if (check->reason)
      fprintf(out, " %s", reason_text(check->reason));
    if (check->card_status)
      fprintf(out, " sw %04x", check->card_status);
    if (check->outcome == TC_PIV_CHECK_FAILED && check->status != TC_CREDENTIAL_INVALID)
      fprintf(out, " %s", example_piv_status_text(check->status));
    fputc('\n', out);
    if (check->outcome < 3)
      ++outcomes[check->outcome];
  }
  fprintf(out, "Summary: %zu passed, %zu failed, %zu not checkable\n", outcomes[0], outcomes[1],
          outcomes[2]);
}
