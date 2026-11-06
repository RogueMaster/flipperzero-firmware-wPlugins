#include "minitest.h"
#include "fixture.h"

#include "proto_decode.h"

#define FIXTURE_CAP 16384

static ProtoMessage msg;
static char text[FIXTURE_CAP];
static char prefix[FIXTURE_CAP];

static ProtoDecodeStatus decode_fixture(const char* name, bool compact) {
    char path[128];
    snprintf(path, sizeof(path), "fixtures/messages/%s", name);
    bool loaded = compact ? fixture_read_compact(path, text, sizeof(text)) :
                            fixture_read(path, text, sizeof(text));
    CHECK(loaded);
    if(!loaded) return ProtoDecodeMalformed;
    return proto_decode_line(text, &msg);
}

static ProtoDecodeStatus decode(const char* line) {
    return proto_decode_line(line, &msg);
}

/* The PLA Basic Jade White spool of the fixtures. */
static void check_spool(const ProtoSpool* spool, int32_t id, unsigned remaining, bool archived) {
    CHECK_EQ_INT(spool->id, id);
    CHECK_STR_EQ(spool->material, "PLA");
    CHECK_STR_EQ(spool->subtype, "Basic");
    CHECK_STR_EQ(spool->color_name, "Jade White");
    CHECK(spool->has_rgba);
    CHECK_EQ_INT(spool->rgba, 0xFFFFFFFFu);
    CHECK_EQ_INT(spool->remaining, remaining);
    CHECK_EQ_INT(spool->label, 1000);
    CHECK_EQ_INT(spool->core, 250);
    CHECK(spool->archived == archived);
}

static void check_lookup_found(void) {
    CHECK(msg.type == ProtoMsgLookup);
    CHECK_STR_EQ(msg.lookup.scan_id, "s-000012");
    CHECK(msg.lookup.status == ProtoLookupFound);
    CHECK_EQ_INT(msg.lookup.spool_id, 42);
    CHECK(!msg.lookup.prefill);
    CHECK_EQ_INT(msg.lookup.lines.count, 4);
    CHECK_STR_EQ(msg.lookup.lines.line[0], "PLA Basic");
    CHECK_STR_EQ(msg.lookup.lines.line[1], "Jade White");
    CHECK_STR_EQ(msg.lookup.lines.line[2], "484 g left of 1000");
    CHECK_STR_EQ(msg.lookup.lines.line[3], "core 250 g");
    CHECK(msg.lookup.defaults.has_gross);
    CHECK(msg.lookup.defaults.has_label);
    CHECK(msg.lookup.defaults.has_core);
    CHECK_EQ_INT(msg.lookup.defaults.gross, 734);
    CHECK_EQ_INT(msg.lookup.defaults.label, 1000);
    CHECK_EQ_INT(msg.lookup.defaults.core, 250);
    CHECK_STR_EQ(msg.lookup.error, "");
    CHECK(msg.lookup.has_spool);
    check_spool(&msg.lookup.spool, 42, 484, false);
    CHECK(!msg.lookup.created);
    CHECK(!msg.lookup.has_tag_info);
}

static void test_lookup_found_fixture(void) {
    CHECK(decode_fixture("lookup_found.json", false) == ProtoDecodeOk);
    check_lookup_found();
    CHECK(decode_fixture("lookup_found.json", true) == ProtoDecodeOk);
    check_lookup_found();
}

static void test_lookup_unknown_fixture(void) {
    CHECK(decode_fixture("lookup_unknown.json", true) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgLookup);
    CHECK_STR_EQ(msg.lookup.scan_id, "s-000012");
    CHECK(msg.lookup.status == ProtoLookupUnknown);
    CHECK_EQ_INT(msg.lookup.spool_id, 0);
    CHECK(msg.lookup.prefill);
    CHECK_EQ_INT(msg.lookup.lines.count, 3);
    CHECK_STR_EQ(msg.lookup.lines.line[0], "PLA Basic");
    CHECK_STR_EQ(msg.lookup.lines.line[1], "#FFFFFFFF");
    CHECK_STR_EQ(msg.lookup.lines.line[2], "label 1000 g");
    CHECK_STR_EQ(msg.lookup.lines.line[3], "");
    CHECK(!msg.lookup.defaults.has_gross);
    CHECK(msg.lookup.defaults.has_label);
    CHECK(msg.lookup.defaults.has_core);
    CHECK_EQ_INT(msg.lookup.defaults.label, 1000);
    CHECK_EQ_INT(msg.lookup.defaults.core, 250);
    CHECK(!msg.lookup.has_spool);
    CHECK(!msg.lookup.created);
    CHECK(msg.lookup.has_tag_info);
    CHECK_STR_EQ(msg.lookup.tag_info.material, "PLA");
    CHECK_STR_EQ(msg.lookup.tag_info.subtype, "Basic");
    CHECK(msg.lookup.tag_info.has_rgba);
    CHECK_EQ_INT(msg.lookup.tag_info.rgba, 0xFFFFFFFFu);
    CHECK(msg.lookup.tag_info.has_label);
    CHECK_EQ_INT(msg.lookup.tag_info.label, 1000);
    CHECK_STR_EQ(msg.lookup.tag_info.color_name, "Jade White");
    CHECK_STR_EQ(msg.lookup.tag_info.code, "10100");
    CHECK_STR_EQ(msg.lookup.tag_info.filament_id, "GFA00");
    CHECK_STR_EQ(msg.lookup.tag_info.diameter, "1.75");
    CHECK(msg.lookup.tag_info.has_nozzle_min);
    CHECK_EQ_INT(msg.lookup.tag_info.nozzle_min, 190);
    CHECK(msg.lookup.tag_info.has_nozzle_max);
    CHECK_EQ_INT(msg.lookup.tag_info.nozzle_max, 230);
}

static void test_lookup_found_archived_fixture(void) {
    CHECK(decode_fixture("lookup_found_archived.json", false) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgLookup);
    CHECK(msg.lookup.status == ProtoLookupFound);
    CHECK_EQ_INT(msg.lookup.spool_id, 42);
    CHECK(msg.lookup.has_spool);
    check_spool(&msg.lookup.spool, 42, 0, true);
    CHECK(!msg.lookup.created);
    CHECK_STR_EQ(msg.lookup.lines.line[2], "archived");
}

static void test_lookup_created_fixture(void) {
    CHECK(decode_fixture("lookup_created.json", true) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgLookup);
    CHECK_STR_EQ(msg.lookup.scan_id, "s-000015");
    CHECK(msg.lookup.status == ProtoLookupFound);
    CHECK_EQ_INT(msg.lookup.spool_id, 31);
    CHECK(msg.lookup.has_spool);
    check_spool(&msg.lookup.spool, 31, 1000, false);
    CHECK(msg.lookup.created);
    CHECK(!msg.lookup.has_tag_info);
    CHECK_EQ_INT(msg.lookup.defaults.gross, 1250);
}

static void test_lookup_unknown_blank_fixture(void) {
    CHECK(decode_fixture("lookup_unknown_blank.json", false) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgLookup);
    CHECK_STR_EQ(msg.lookup.scan_id, "s-000013");
    CHECK(msg.lookup.status == ProtoLookupUnknown);
    CHECK(!msg.lookup.prefill);
    CHECK(!msg.lookup.has_tag_info);
    CHECK_STR_EQ(msg.lookup.tag_info.material, "");
    CHECK(!msg.lookup.has_spool);
    CHECK_EQ_INT(msg.lookup.lines.count, 2);
    CHECK_STR_EQ(msg.lookup.lines.line[0], "NTAG");
    CHECK_STR_EQ(msg.lookup.lines.line[1], "UID 04112233445566");
}

static void test_lookup_error_fixture(void) {
    CHECK(decode_fixture("lookup_error.json", false) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgLookup);
    CHECK(msg.lookup.status == ProtoLookupError);
    CHECK_EQ_INT(msg.lookup.lines.count, 1);
    CHECK_STR_EQ(msg.lookup.lines.line[0], "Bambuddy unreachable");
    CHECK_STR_EQ(msg.lookup.error, "connection timed out");
    CHECK(!msg.lookup.defaults.has_gross);
    CHECK(!msg.lookup.defaults.has_label);
    CHECK(!msg.lookup.defaults.has_core);
}

static void test_result_ok_fixture(void) {
    CHECK(decode_fixture("result_ok.json", false) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgResult);
    CHECK_STR_EQ(msg.result.id, "i-000003");
    CHECK(msg.result.ok);
    CHECK_EQ_INT(msg.result.lines.count, 2);
    CHECK_STR_EQ(msg.result.lines.line[0], "Saved");
    CHECK_STR_EQ(msg.result.lines.line[1], "484 g left");
    CHECK_STR_EQ(msg.result.error, "");
    CHECK(msg.result.has_spool);
    check_spool(&msg.result.spool, 42, 484, false);
}

static void test_result_spool_fixtures(void) {
    CHECK(decode_fixture("result_new.json", true) == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.id, "i-000004");
    CHECK(msg.result.ok);
    CHECK(msg.result.has_spool);
    check_spool(&msg.result.spool, 31, 1000, false);

    CHECK(decode_fixture("result_full.json", false) == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.id, "i-000008");
    CHECK(msg.result.has_spool);
    check_spool(&msg.result.spool, 42, 1000, false);
    CHECK_STR_EQ(msg.result.lines.line[0], "Marked as new");

    CHECK(decode_fixture("result_empty.json", true) == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.id, "i-000009");
    CHECK(msg.result.has_spool);
    check_spool(&msg.result.spool, 42, 0, true);

    CHECK(decode_fixture("result_delete.json", false) == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.id, "i-000010");
    CHECK(msg.result.ok);
    CHECK(!msg.result.has_spool);
    CHECK_EQ_INT(msg.result.spool.id, 0);
    CHECK_STR_EQ(msg.result.lines.line[0], "Deleted");
}

static void test_result_error_fixture(void) {
    CHECK(decode_fixture("result_error.json", true) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgResult);
    CHECK_STR_EQ(msg.result.id, "i-000003");
    CHECK(!msg.result.ok);
    CHECK_EQ_INT(msg.result.lines.count, 1);
    CHECK_STR_EQ(msg.result.lines.line[0], "Not saved");
    CHECK_STR_EQ(msg.result.error, "spool 42 not found");
    CHECK(!msg.result.has_spool);
}

static void test_hello_fixtures(void) {
    CHECK(decode_fixture("hello_host.json", false) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgHello);
    CHECK_STR_EQ(msg.hello.app, "flibuddy-mac/0.1.0");
    CHECK_STR_EQ(msg.hello.os, "macOS 26.0");
    CHECK_STR_EQ(msg.hello.fw, "");

    CHECK(decode_fixture("hello_flipper.json", true) == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgHello);
    CHECK_STR_EQ(msg.hello.app, "flibuddy-fap/1.0");
    CHECK_STR_EQ(msg.hello.fw, "1.5.1");
    CHECK_STR_EQ(msg.hello.os, "");
}

static void test_flipper_bound_messages_are_ignored(void) {
    /* scan and intent travel Flipper -> host; the Flipper ignores them. */
    CHECK(decode_fixture("scan_mfc.json", true) == ProtoDecodeIgnored);
    CHECK(msg.type == ProtoMsgNone);
    static const char* const names[] = {
        "scan_mfc_autosave.json",
        "scan_ntag.json",
        "intent_weigh.json",
        "intent_new.json",
        "intent_full.json",
        "intent_empty.json",
        "intent_delete.json",
        "intent_create_on_host.json",
        "intent_link_on_host.json",
    };
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        CHECK(decode_fixture(names[i], true) == ProtoDecodeIgnored);
        CHECK(msg.type == ProtoMsgNone);
    }
}

static void test_invalid_fixtures(void) {
    CHECK(decode_fixture("invalid_missing_t.json", false) == ProtoDecodeMalformed);
    CHECK(msg.type == ProtoMsgNone);
    /* Invalid Flipper -> host messages are still not the Flipper's to read. */
    CHECK(decode_fixture("invalid_bad_action.json", false) == ProtoDecodeIgnored);
    CHECK(decode_fixture("invalid_scan_no_autosave.json", false) == ProtoDecodeIgnored);
    CHECK(decode_fixture("invalid_weigh_no_grams.json", false) == ProtoDecodeIgnored);
    CHECK(msg.type == ProtoMsgNone);
}

#define SPOOL_HEAD \
    "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"found\",\"spool_id\":7,"
#define RESULT_HEAD "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,\"lines\":[],"
#define UNKNOWN_HEAD \
    "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\",\"lines\":[],"

static void test_spool_minimal_and_any_order(void) {
    CHECK(
        decode(SPOOL_HEAD
               "\"lines\":[],\"spool\":{\"core\":0,\"label\":9999,"
               "\"remaining\":5,\"material\":\"PETG\",\"id\":7,\"note\":[1]}}") == ProtoDecodeOk);
    CHECK(msg.lookup.has_spool);
    CHECK_EQ_INT(msg.lookup.spool.id, 7);
    CHECK_STR_EQ(msg.lookup.spool.material, "PETG");
    CHECK_STR_EQ(msg.lookup.spool.subtype, "");
    CHECK_STR_EQ(msg.lookup.spool.color_name, "");
    CHECK(!msg.lookup.spool.has_rgba);
    CHECK_EQ_INT(msg.lookup.spool.remaining, 5);
    CHECK_EQ_INT(msg.lookup.spool.label, 9999);
    CHECK_EQ_INT(msg.lookup.spool.core, 0);
    CHECK(!msg.lookup.spool.archived);
    CHECK(!msg.lookup.created);

    /* the spool before spool_id, created false, archived false */
    CHECK(
        decode("{\"t\":\"lookup\",\"v\":2,\"spool\":{\"id\":3,\"material\":\"PLA\","
               "\"remaining\":1,\"label\":2,\"core\":3,\"archived\":false,"
               "\"rgba\":\"0A1B2C3D\"},\"created\":false,\"status\":\"found\","
               "\"spool_id\":3,\"scan_id\":\"s-1\",\"lines\":[]}") == ProtoDecodeOk);
    CHECK_EQ_INT(msg.lookup.spool.id, 3);
    CHECK(msg.lookup.spool.has_rgba);
    CHECK_EQ_INT(msg.lookup.spool.rgba, 0x0A1B2C3Du);
    CHECK(!msg.lookup.spool.archived);
    CHECK(!msg.lookup.created);

    /* found without spool still decodes: the Flipper falls back to lines */
    CHECK(decode(SPOOL_HEAD "\"lines\":[\"PLA\"]}") == ProtoDecodeOk);
    CHECK(!msg.lookup.has_spool);
}

static void test_spool_text_is_bounded_ascii(void) {
    CHECK(
        decode(RESULT_HEAD "\"spool\":{\"id\":1,\"material\":\"PLA\","
                           "\"subtype\":\"0123456789012345678901234567890123456789\","
                           "\"color_name\":\"Gr\xC3\xBCn \\u00e9\",\"remaining\":0,"
                           "\"label\":0,\"core\":0}}") == ProtoDecodeOk);
    CHECK(msg.result.has_spool);
    CHECK_STR_EQ(msg.result.spool.subtype, "0123456789012345678901234567890123456789");
    CHECK_STR_EQ(msg.result.spool.color_name, "Gr?n ?");
}

static void test_tag_info_fields_are_optional(void) {
    CHECK(decode(UNKNOWN_HEAD "\"tag_info\":{}}") == ProtoDecodeOk);
    CHECK(msg.lookup.has_tag_info);
    CHECK_STR_EQ(msg.lookup.tag_info.material, "");
    CHECK_STR_EQ(msg.lookup.tag_info.subtype, "");
    CHECK(!msg.lookup.tag_info.has_rgba);
    CHECK(!msg.lookup.tag_info.has_label);
    CHECK(
        decode(UNKNOWN_HEAD "\"tag_info\":{\"label\":0,\"x\":1,\"rgba\":\"00000000\"}}") ==
        ProtoDecodeOk);
    CHECK(msg.lookup.tag_info.has_label);
    CHECK_EQ_INT(msg.lookup.tag_info.label, 0);
    CHECK(msg.lookup.tag_info.has_rgba);
    CHECK_EQ_INT(msg.lookup.tag_info.rgba, 0);
}

static void test_malformed_v2_fields(void) {
    static const char* const lines[] = {
        /* lookup.spool */
        SPOOL_HEAD "\"lines\":[],\"spool\":[]}",
        SPOOL_HEAD "\"lines\":[],\"spool\":null}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"material\":\"PLA\",\"remaining\":1,"
                   "\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"remaining\":1,\"label\":1,"
                   "\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"label\":1,"
                   "\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":1,"
                   "\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":1,"
                   "\"label\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":0,\"material\":\"PLA\",\"remaining\":1,"
                   "\"label\":1,\"core\":1}}",
        /* spool.id must equal spool_id */
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":8,\"material\":\"PLA\",\"remaining\":1,"
                   "\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"\",\"remaining\":1,"
                   "\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":"
                   "\"01234567890123456789012345678901234567890\",\"remaining\":1,"
                   "\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"subtype\":\"\","
                   "\"remaining\":1,\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"color_name\":7,"
                   "\"remaining\":1,\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"rgba\":\"ffffffff\","
                   "\"remaining\":1,\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"rgba\":\"FFFFFFF\","
                   "\"remaining\":1,\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"rgba\":\"FFFFFFFFF\","
                   "\"remaining\":1,\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"rgba\":\"FFFFFFFG\","
                   "\"remaining\":1,\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":10000,"
                   "\"label\":1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":1,"
                   "\"label\":-1,\"core\":1}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":1,"
                   "\"label\":1,\"core\":\"1\"}}",
        SPOOL_HEAD "\"lines\":[],\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":1,"
                   "\"label\":1,\"core\":1,\"archived\":1}}",
        /* lookup.created */
        SPOOL_HEAD "\"lines\":[],\"created\":1}",
        SPOOL_HEAD "\"lines\":[],\"created\":\"true\"}",
        UNKNOWN_HEAD "\"created\":true}",
        UNKNOWN_HEAD "\"created\":false}",
        /* lookup.spool only with found */
        UNKNOWN_HEAD "\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":1,\"label\":1,"
                     "\"core\":1}}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"error\",\"lines\":[],"
        "\"error\":\"x\",\"spool\":{\"id\":7,\"material\":\"PLA\",\"remaining\":1,"
        "\"label\":1,\"core\":1}}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"error\",\"lines\":[],"
        "\"error\":\"x\",\"created\":true}",
        /* lookup.tag_info only with unknown, well formed */
        SPOOL_HEAD "\"lines\":[],\"tag_info\":{}}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"error\",\"lines\":[],"
        "\"error\":\"x\",\"tag_info\":{}}",
        UNKNOWN_HEAD "\"tag_info\":[]}",
        UNKNOWN_HEAD "\"tag_info\":{\"material\":\"\"}}",
        UNKNOWN_HEAD "\"tag_info\":{\"subtype\":1}}",
        UNKNOWN_HEAD "\"tag_info\":{\"rgba\":\"FFFFFFFFFF\"}}",
        UNKNOWN_HEAD "\"tag_info\":{\"rgba\":\"#FFFFFFF\"}}",
        UNKNOWN_HEAD "\"tag_info\":{\"label\":10000}}",
        UNKNOWN_HEAD "\"tag_info\":{\"label\":true}}",
        /* result.spool */
        RESULT_HEAD "\"spool\":{}}",
        RESULT_HEAD "\"spool\":\"PLA\"}",
        RESULT_HEAD "\"spool\":{\"id\":-1,\"material\":\"PLA\",\"remaining\":1,\"label\":1,"
                    "\"core\":1}}",
        /* result.spool only with ok: true */
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":false,\"lines\":[],\"error\":\"x\","
        "\"spool\":{\"id\":1,\"material\":\"PLA\",\"remaining\":1,\"label\":1,\"core\":1}}",
    };
    for(size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        ProtoDecodeStatus status = decode(lines[i]);
        if(status != ProtoDecodeMalformed)
            MINITEST_FAIL("line %zu decoded as %d, expected malformed: %s", i, status, lines[i]);
        minitest_checks++;
        CHECK(msg.type == ProtoMsgNone);
    }
}

static void test_unknown_type_or_version_is_ignored(void) {
    CHECK(decode("{\"t\":\"ping\",\"v\":2}") == ProtoDecodeIgnored);
    CHECK(decode("{\"t\":\"ping\",\"v\":2,\"anything\":[{\"x\":null}]}") == ProtoDecodeIgnored);
    /* v1 and v3 peers are silent to a v2 Flipper, however valid the body. */
    CHECK(
        decode("{\"v\":1,\"t\":\"result\",\"id\":\"i-1\",\"ok\":true,\"lines\":[]}") ==
        ProtoDecodeIgnored);
    CHECK(
        decode("{\"t\":\"result\",\"v\":3,\"id\":\"i-1\",\"ok\":true,\"lines\":[]}") ==
        ProtoDecodeIgnored);
    CHECK(
        decode("{\"t\":\"lookup\",\"v\":1,\"scan_id\":\"s-1\",\"status\":\"unknown\","
               "\"lines\":[]}") == ProtoDecodeIgnored);
    CHECK(msg.type == ProtoMsgNone);
    /* An unknown type is still dropped as malformed when it is not JSON. */
    CHECK(decode("{\"t\":\"ping\",\"v\":2,") == ProtoDecodeMalformed);
}

static void test_hello_of_another_version_is_reported(void) {
    /* the waiting screen names the version a host app speaks */
    CHECK(
        decode("{\"t\":\"hello\",\"v\":3,\"app\":\"flibuddy-mac/9.0.0\"}") ==
        ProtoDecodeOtherVersion);
    CHECK(msg.type == ProtoMsgOtherVersion);
    CHECK_EQ_INT(msg.other_version, 3);
    CHECK(
        decode("{\"v\":1,\"app\":\"flibuddy-mac/0.0.1\",\"t\":\"hello\"}") ==
        ProtoDecodeOtherVersion);
    CHECK_EQ_INT(msg.other_version, 1);
    /* only a hello says what the app speaks; other messages stay ignored */
    CHECK(
        decode("{\"t\":\"result\",\"v\":3,\"id\":\"i-1\",\"ok\":true,\"lines\":[]}") ==
        ProtoDecodeIgnored);
    CHECK(msg.type == ProtoMsgNone);
    CHECK_EQ_INT(msg.other_version, 0);
    /* a version that is no small positive number is noise */
    CHECK(decode("{\"t\":\"hello\",\"v\":0}") == ProtoDecodeIgnored);
    CHECK(decode("{\"t\":\"hello\",\"v\":100}") == ProtoDecodeIgnored);
    CHECK(decode("{\"t\":\"hello\",\"v\":-2}") == ProtoDecodeIgnored);
    /* the current version is a normal hello */
    CHECK(
        decode("{\"t\":\"hello\",\"v\":2,\"app\":\"flibuddy-mac/1\",\"os\":\"macOS\"}") ==
        ProtoDecodeOk);
    CHECK_EQ_INT(msg.other_version, 0);
}

static void test_key_order_and_unknown_fields(void) {
    CHECK(
        decode("{\"lines\":[\"x\"],\"extra\":{\"a\":[1,2.5e3,null]},\"ok\":true,"
               "\"id\":\"i-9\",\"v\":2,\"t\":\"result\"}") == ProtoDecodeOk);
    CHECK(msg.type == ProtoMsgResult);
    CHECK_STR_EQ(msg.result.id, "i-9");
    CHECK(msg.result.ok);
    CHECK_EQ_INT(msg.result.lines.count, 1);
    /* A 32-character id is the longest allowed. */
    CHECK(
        decode("{\"t\":\"result\",\"v\":2,\"id\":\"i-123456789012345678901234567890\","
               "\"ok\":true,\"lines\":[]}") == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.id, "i-123456789012345678901234567890");
    CHECK(
        decode("{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\","
               "\"lines\":[],\"defaults\":{\"core\":0,\"tare\":5}}") == ProtoDecodeOk);
    CHECK(msg.lookup.defaults.has_core);
    CHECK_EQ_INT(msg.lookup.defaults.core, 0);
    CHECK_EQ_INT(msg.lookup.lines.count, 0);
}

static void test_line_length_cap(void) {
    /* 40 chars fit, 41 do not. */
    CHECK(
        decode("{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,"
               "\"lines\":[\"0123456789012345678901234567890123456789\"]}") == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.lines.line[0], "0123456789012345678901234567890123456789");
    CHECK(
        decode("{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,"
               "\"lines\":[\"01234567890123456789012345678901234567890\"]}") ==
        ProtoDecodeMalformed);
    CHECK(msg.type == ProtoMsgNone);
    /* Non-ASCII renders as '?', one per character. */
    CHECK(
        decode("{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,"
               "\"lines\":[\"220\xC2\xB0\x43 \\\"hot\\\"\"]}") == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.lines.line[0], "220?C \"hot\"");
}

static void test_lines_count_cap(void) {
    CHECK(
        decode("{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,"
               "\"lines\":[\"a\",\"b\",\"c\",\"d\"]}") == ProtoDecodeOk);
    CHECK_EQ_INT(msg.result.lines.count, 4);
    CHECK(
        decode("{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,"
               "\"lines\":[\"a\",\"b\",\"c\",\"d\",\"e\"]}") == ProtoDecodeMalformed);
}

static void test_error_length_cap(void) {
    char line[512];
    char error[202];
    memset(error, 'e', 200);
    error[200] = '\0';
    snprintf(
        line,
        sizeof(line),
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":false,\"lines\":[],\"error\":\"%s\"}",
        error);
    CHECK(decode(line) == ProtoDecodeOk);
    CHECK_STR_EQ(msg.result.error, error);
    error[200] = 'e';
    error[201] = '\0';
    snprintf(
        line,
        sizeof(line),
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":false,\"lines\":[],\"error\":\"%s\"}",
        error);
    CHECK(decode(line) == ProtoDecodeMalformed);
}

static void test_malformed_lines(void) {
    static const char* const lines[] = {
        "",
        "   ",
        "not json",
        "[]",
        "\"lookup\"",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,\"lines\":[]} x",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,\"lines\":[]}{}",
        /* header */
        "{\"v\":2}",
        "{\"t\":1,\"v\":2}",
        "{\"t\":\"result\"}",
        "{\"t\":\"result\",\"v\":\"1\",\"id\":\"i-1\",\"ok\":true,\"lines\":[]}",
        "{\"t\":\"result\",\"v\":2.0,\"id\":\"i-1\",\"ok\":true,\"lines\":[]}",
        /* result */
        "{\"t\":\"result\",\"v\":2,\"ok\":true,\"lines\":[]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i 1\",\"ok\":true,\"lines\":[]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"\",\"ok\":true,\"lines\":[]}",
        /* 33-character id */
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1234567890123456789012345678901\",\"ok\":true,"
        "\"lines\":[]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"lines\":[]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":1,\"lines\":[]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,\"lines\":\"Saved\"}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,\"lines\":[1]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,\"lines\":[null]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":false,\"lines\":[]}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":false,\"lines\":[],\"error\":\"\"}",
        "{\"t\":\"result\",\"v\":2,\"id\":\"i-1\",\"ok\":true,\"lines\":[],\"error\":7}",
        /* lookup */
        "{\"t\":\"lookup\",\"v\":2,\"status\":\"unknown\",\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"lost\",\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\"}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"found\",\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"found\",\"spool_id\":0,"
        "\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"found\",\"spool_id\":\"42\","
        "\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"found\","
        "\"spool_id\":2147483648,\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"error\",\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\",\"prefill\":1,"
        "\"lines\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\",\"lines\":[],"
        "\"defaults\":[]}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\",\"lines\":[],"
        "\"defaults\":{\"gross\":10000}}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\",\"lines\":[],"
        "\"defaults\":{\"core\":-1}}",
        "{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-1\",\"status\":\"unknown\",\"lines\":[],"
        "\"defaults\":{\"label\":\"1000\"}}",
        /* hello */
        "{\"t\":\"hello\",\"v\":2,\"os\":\"macOS\"}",
        "{\"t\":\"hello\",\"v\":2,\"app\":\"\",\"os\":\"macOS\"}",
        "{\"t\":\"hello\",\"v\":2,\"app\":\"mac\"}",
        "{\"t\":\"hello\",\"v\":2,\"app\":\"mac\",\"os\":\"macOS\",\"fw\":\"1.5.1\"}",
        "{\"t\":\"hello\",\"v\":2,\"app\":\"mac\",\"os\":\"\"}",
        "{\"t\":\"hello\",\"v\":2,\"app\":\"mac\","
        "\"os\":\"01234567890123456789012345678901234567890\"}",
    };
    for(size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        ProtoDecodeStatus status = decode(lines[i]);
        if(status != ProtoDecodeMalformed)
            MINITEST_FAIL("line %zu decoded as %d, expected malformed: %s", i, status, lines[i]);
        minitest_checks++;
        CHECK(msg.type == ProtoMsgNone);
    }
}

static void test_message_is_cleared_between_decodes(void) {
    CHECK(decode_fixture("lookup_found.json", true) == ProtoDecodeOk);
    CHECK(
        decode("{\"t\":\"lookup\",\"v\":2,\"scan_id\":\"s-2\",\"status\":\"unknown\","
               "\"lines\":[\"x\"]}") == ProtoDecodeOk);
    CHECK_EQ_INT(msg.lookup.spool_id, 0);
    CHECK(!msg.lookup.defaults.has_gross);
    CHECK_STR_EQ(msg.lookup.lines.line[1], "");
    CHECK(!msg.lookup.has_spool);
    CHECK_STR_EQ(msg.lookup.spool.material, "");
    CHECK(decode_fixture("lookup_unknown.json", true) == ProtoDecodeOk);
    CHECK(decode_fixture("lookup_unknown_blank.json", true) == ProtoDecodeOk);
    CHECK(!msg.lookup.has_tag_info);
    CHECK(!msg.lookup.tag_info.has_label);
}

/* No truncation of a valid line decodes, and none crashes (ASan/UBSan). */
static void test_every_prefix_is_malformed(void) {
    static const char* const names[] = {
        "lookup_found.json",
        "lookup_unknown.json",
        "lookup_error.json",
        "lookup_found_archived.json",
        "lookup_created.json",
        "lookup_unknown_blank.json",
        "result_ok.json",
        "result_new.json",
        "result_full.json",
        "result_empty.json",
        "result_delete.json",
        "result_error.json",
        "hello_host.json",
    };
    for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char path[128];
        snprintf(path, sizeof(path), "fixtures/messages/%s", names[i]);
        CHECK(fixture_read_compact(path, text, sizeof(text)));
        size_t len = strlen(text);
        bool all_malformed = true;
        for(size_t n = 0; n < len; n++) {
            memcpy(prefix, text, n);
            prefix[n] = '\0';
            if(proto_decode_line(prefix, &msg) != ProtoDecodeMalformed) {
                MINITEST_FAIL("%s: prefix of %zu bytes did not fail", names[i], n);
                all_malformed = false;
            }
        }
        CHECK(all_malformed);
        CHECK(proto_decode_line(text, &msg) == ProtoDecodeOk);
    }
}

int main(void) {
    RUN_TEST(test_lookup_found_fixture);
    RUN_TEST(test_lookup_unknown_fixture);
    RUN_TEST(test_lookup_found_archived_fixture);
    RUN_TEST(test_lookup_created_fixture);
    RUN_TEST(test_lookup_unknown_blank_fixture);
    RUN_TEST(test_lookup_error_fixture);
    RUN_TEST(test_result_ok_fixture);
    RUN_TEST(test_result_spool_fixtures);
    RUN_TEST(test_result_error_fixture);
    RUN_TEST(test_hello_fixtures);
    RUN_TEST(test_flipper_bound_messages_are_ignored);
    RUN_TEST(test_invalid_fixtures);
    RUN_TEST(test_spool_minimal_and_any_order);
    RUN_TEST(test_spool_text_is_bounded_ascii);
    RUN_TEST(test_tag_info_fields_are_optional);
    RUN_TEST(test_malformed_v2_fields);
    RUN_TEST(test_unknown_type_or_version_is_ignored);
    RUN_TEST(test_hello_of_another_version_is_reported);
    RUN_TEST(test_key_order_and_unknown_fields);
    RUN_TEST(test_line_length_cap);
    RUN_TEST(test_lines_count_cap);
    RUN_TEST(test_error_length_cap);
    RUN_TEST(test_malformed_lines);
    RUN_TEST(test_message_is_cleared_between_decodes);
    RUN_TEST(test_every_prefix_is_malformed);
    return MINITEST_RESULT();
}
