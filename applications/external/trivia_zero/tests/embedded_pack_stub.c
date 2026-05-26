/* Host-test stub: provides minimal valid embedded_pack symbols so unit
 * tests that link pack_reader.o satisfy the linker without needing the
 * generated src/data/embedded_pack_*.c files (which only exist after
 * `make pack`). The stubs declare a valid IDX header with count=0, so
 * pack_open() returns true but pack_count() returns 0 — perfectly fine
 * for tests that exercise the pure parsers. */

#include "include/data/embedded_pack.h"

const uint8_t trivia_es_idx[] = {
    'T',  'R',  'V',  'I',  /* magic */
    0x01, 0x00,             /* version 1 LE */
    0x00, 0x00, 0x00, 0x00, /* count = 0 */
};
const size_t trivia_es_idx_len = sizeof(trivia_es_idx);
const char trivia_es_tsv[] = "";
const size_t trivia_es_tsv_len = 0u;

const uint8_t trivia_en_idx[] = {
    'T', 'R', 'V', 'I', 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const size_t trivia_en_idx_len = sizeof(trivia_en_idx);
const char trivia_en_tsv[] = "";
const size_t trivia_en_tsv_len = 0u;
