/* Host: cc -std=c11 -Wall -Wextra -Werror -pedantic -I. \
 * tests/uhf_saved_tag_test.c uhf_saved_tag.c uhf_tag.c uhf_protocol.c \
 * -o /tmp/uhf_saved_tag_test && /tmp/uhf_saved_tag_test */
#include "uhf_saved_tag.h"
#include "uhf_tag.h"

#include <assert.h>
#include <string.h>

static const char* const normal_record =
    "Filetype: UHF Tag\n"
    "Version: 1\n"
    "EPC: 300833B2DDD9014000000001\n"
    "PC: 3000\n"
    "RSSI: 42\n"
    "TID: E2801105200030A5\n"
    "USER: \n"
    "Timestamp: 2026-11-11 05:48:00\n"
    "Name: Crate\n";

static void test_epc_length(void) {
    assert(uhf_tag_epc_length_ok(0U));
    assert(!uhf_tag_epc_length_ok(2U));
    assert(!uhf_tag_epc_length_ok(3U));
    assert(uhf_tag_epc_length_ok(4U));
    assert(!uhf_tag_epc_length_ok(5U));
    assert(uhf_tag_epc_length_ok(48U));
    assert(!uhf_tag_epc_length_ok(50U));
}

static void test_normal_record(void) {
    UhfSavedTag tag;
    char text[UHF_SAVED_TAG_TEXT_MAX];
    assert(uhf_saved_tag_parse(normal_record, &tag));
    assert(strcmp(tag.epc, "300833B2DDD9014000000001") == 0);
    assert(strcmp(tag.tid, "E2801105200030A5") == 0);
    assert(strcmp(tag.name, "Crate") == 0);
    assert(tag.pc == 0x3000U);
    assert(tag.rssi == 42U);
    assert(uhf_saved_tag_format(&tag, text, sizeof(text)));
    assert(uhf_saved_tag_parse(text, &tag));
    assert(strcmp(tag.epc, "300833B2DDD9014000000001") == 0);
}

static void test_tid_length(void) {
    const uint8_t ucode8[] = {
        0xE2, 0x80, 0x68, 0x94, 0x20, 0x00, 0x40, 0x2E, 0x25, 0x67, 0x11, 0x22, 0x06, 0xEE};
    const uint8_t short_header[] = {0xE2, 0x80, 0x68, 0x94, 0x20, 0x00, 0x40, 0x2E};
    const uint8_t no_serial[] = {0xE2, 0x80, 0x11, 0x05, 0x00, 0x00, 0x30, 0xA5};
    const uint8_t extended[] = {0xE2, 0x80, 0x68, 0x94, 0xA0, 0x00, 0x40, 0x2E};

    assert(uhf_tag_tid_length(ucode8, sizeof(ucode8)) == 12U);
    assert(uhf_tag_tid_length(short_header, sizeof(short_header)) == 12U);
    assert(uhf_tag_tid_length(no_serial, sizeof(no_serial)) == 0U);
    assert(uhf_tag_tid_length(extended, sizeof(extended)) == 0U);
    assert(uhf_tag_tid_length(ucode8, 4U) == 0U);
    assert(uhf_tag_tid_length(NULL, 8U) == 0U);
}

static void test_tid_only_record(void) {
    UhfSavedTag tag = {.name = "TID only", .tid = "E280110520007A3B", .pc = 0U, .rssi = 7U};
    UhfSavedTag loaded;
    char text[UHF_SAVED_TAG_TEXT_MAX];
    const char* both_empty =
        "Filetype: UHF Tag\nVersion: 1\nEPC: \nPC: 0000\nRSSI: 0\nTID: \nName: Blank\n";

    assert(uhf_saved_tag_format(&tag, text, sizeof(text)));
    assert(uhf_saved_tag_parse(text, &loaded));
    assert(loaded.epc[0] == '\0');
    assert(strcmp(loaded.tid, "E280110520007A3B") == 0);
    assert(strcmp(loaded.name, "TID only") == 0);
    assert(loaded.rssi == 7U);

    tag.tid[0] = '\0';
    assert(!uhf_saved_tag_format(&tag, text, sizeof(text)));
    assert(!uhf_saved_tag_parse(both_empty, &loaded));
}

int main(void) {
    test_epc_length();
    test_tid_length();
    test_normal_record();
    test_tid_only_record();
    return 0;
}
