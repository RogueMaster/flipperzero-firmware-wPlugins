/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "piv_inspect_print.h"
#include <tiny_crypto/fascn.h>

static void print_time(FILE* out, const TC_X509_time* time)
{
  fprintf(out, "%04u-%02u-%02uT%02u:%02u:%02uZ", time->year, time->month, time->day, time->hour,
          time->minute, time->second);
}

/* A 16-byte UUID in RFC 9562 text form. */
static void print_uuid(FILE* out, TC_bytes uuid)
{
  for (size_t i = 0; i < uuid.length; ++i)
    fprintf(out, "%s%02x", i == 4 || i == 6 || i == 8 || i == 10 ? "-" : "", uuid.data[i]);
}

/* The FASC-N fields (SP 800-73-5 Part 1 section 3.1.2, TIG SCEPACS). */
static void print_fascn(FILE* out, TC_bytes encoded)
{
  TC_FASCN fascn;
  if (TC_FASCN_read(encoded, &fascn) != TC_TLV_OK) {
    fputs("malformed", out);
    return;
  }
  fprintf(out, "agency %04u system %04u credential %06lu series %u issue %u person %010llu",
          fascn.agency, fascn.system, (unsigned long)fascn.credential, fascn.series, fascn.issue,
          (unsigned long long)fascn.person);
}

static void print_certificate(FILE* out, const char* slot, const TC_X509_certificate* certificate,
                              uint8_t valid)
{
  fprintf(out, "  %s: serial ", slot);
  example_piv_print_hex(out, certificate->serial);
  fputs(", valid ", out);
  print_time(out, &certificate->not_before);
  fputs(" to ", out);
  print_time(out, &certificate->not_after);
  const TC_X509_public_key* key = &certificate->public_key;
  fprintf(out, ", %s %u, %s\n", key->type == TC_KEY_EC ? "EC" : "RSA", key->bits,
          valid ? "accepted" : "not accepted");
}

void example_piv_print_certificates(FILE* out, const TC_PIV_card_report* report)
{
  static const char* const slots[] = {"9A", "9C", "9D", "9E"};
  fputs("Evaluated at ", out);
  print_time(out, &report->at);
  fputc('\n', out);
  if (report->has_card) {
    fputs("Card: FASC-N ", out);
    print_fascn(out, report->card.fascn);
    fputs("\n  card UUID ", out);
    example_piv_print_text(out, report->card.uuid_urn);
    fputs(", expires ", out);
    print_time(out, &report->card_expiration);
    fputc('\n', out);
  }
  if (report->has_chuid) {
    fputs("CHUID: GUID ", out);
    print_uuid(out, report->chuid.object.card_uuid);
    fputs(", expiration ", out);
    example_piv_print_text(out, report->chuid.object.expiration);
    fputc('\n', out);
  }
  fputs("Certificates:\n", out);
  for (size_t slot = 0; slot < TC_PIV_CARD_CERTIFICATES; ++slot)
    if (report->certificates[slot].encoded.length)
      print_certificate(out, slots[slot], &report->certificates[slot],
                        report->certificate_valid[slot]);
}
