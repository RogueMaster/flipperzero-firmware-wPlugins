/* Intel-HEX Parser/Builder (puffer-basiert, Storage-agnostisch). */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* Text -> img (Groesse img_cap, wird mit 0xFF vorbelegt). 0 = ok, -1 = Fehler.
 * out_len = hoechste belegte Adresse + 1. */
int ihex_parse(const char* text, size_t len, uint8_t* img, uint32_t img_cap, uint32_t* out_len);

/* Eine Datenzeile ":LL AAAA 00 <n Bytes> CC" + CRLF nach out. Rueckgabe: Zeichen. */
int ihex_record(char* out, size_t cap, uint16_t addr, const uint8_t* data, uint8_t n);

/* EOF-Zeile ":00000001FF" + CRLF. */
int ihex_eof(char* out, size_t cap);
