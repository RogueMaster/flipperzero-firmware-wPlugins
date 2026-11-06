#include "minitest.h"
#include "fixture.h"
#include "dump_loaders.h"

#include "intents.h"

#define FIXTURE_CAP 16384

static Scan mfc_scan;
static Scan ntag_scan;
static Intent intent;
static Intent other;
static SingleSlotIntentStore slot;
static char line[PROTO_LINE_MAX];
static char expected[FIXTURE_CAP];

static void load_scans(void) {
    load_mfc(&mfc_scan.tag);
    strcpy(mfc_scan.id, MFC_SCAN_ID);
    load_ntag(&ntag_scan.tag);
    strcpy(ntag_scan.id, NTAG_SCAN_ID);
}

static void check_matches_fixture(const Intent* i, const char* fixture) {
    char path[128];
    snprintf(path, sizeof(path), "fixtures/messages/%s", fixture);
    size_t len = intent_encode(i, line, sizeof(line));
    CHECK(fixture_read_compact(path, expected, sizeof(expected)));
    CHECK_STR_EQ(line, expected);
    CHECK_EQ_INT(len, strlen(expected));
}

static void test_id_seq_formats_zero_padded_counter(void) {
    IdSeq seq;
    char id[PROTO_ID_MAX + 1];
    id_seq_init(&seq, 'i', 1);
    CHECK(id_seq_next(&seq, id));
    CHECK_STR_EQ(id, "i-000001");
    CHECK(id_seq_next(&seq, id));
    CHECK_STR_EQ(id, "i-000002");

    id_seq_init(&seq, 's', 999999);
    CHECK(id_seq_next(&seq, id));
    CHECK_STR_EQ(id, "s-999999");
    CHECK(id_seq_next(&seq, id));
    CHECK_STR_EQ(id, "s-1000000");
    CHECK(proto_id_valid(id));

    id_seq_init(&seq, 'i', UINT32_MAX);
    CHECK(id_seq_next(&seq, id));
    CHECK_STR_EQ(id, "i-4294967295");
    CHECK(proto_id_valid(id));
    CHECK(id_seq_next(&seq, id)); /* wraps instead of failing */
    CHECK_STR_EQ(id, "i-000000");
}

static void test_weigh_matches_fixture(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 3);
    CHECK(intent_new_weigh(&intent, &ids, &mfc_scan, 734, WeightModeGross));
    CHECK(intent.action == IntentActionWeigh);
    CHECK_STR_EQ(intent.id, "i-000003");
    CHECK_STR_EQ(intent.scan_id, MFC_SCAN_ID);
    CHECK_EQ_INT(intent.grams, 734);
    CHECK(intent.mode == WeightModeGross);
    check_matches_fixture(&intent, "intent_weigh.json");
}

static void test_other_actions_match_fixtures(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 4);
    CHECK(intent_new_action(&intent, &ids, &mfc_scan, IntentActionNew));
    check_matches_fixture(&intent, "intent_new.json");
    CHECK(intent_new_action(&intent, &ids, &ntag_scan, IntentActionCreateOnHost));
    check_matches_fixture(&intent, "intent_create_on_host.json");
    CHECK(intent_new_action(&intent, &ids, &ntag_scan, IntentActionLinkOnHost));
    check_matches_fixture(&intent, "intent_link_on_host.json");
}

static void test_spool_actions_match_fixtures(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 8);
    CHECK(intent_new_action(&intent, &ids, &mfc_scan, IntentActionFull));
    CHECK(intent.action == IntentActionFull);
    check_matches_fixture(&intent, "intent_full.json");
    CHECK(intent_new_action(&intent, &ids, &mfc_scan, IntentActionEmpty));
    CHECK(intent.action == IntentActionEmpty);
    check_matches_fixture(&intent, "intent_empty.json");
    CHECK(intent_new_action(&intent, &ids, &mfc_scan, IntentActionDelete));
    CHECK(intent.action == IntentActionDelete);
    check_matches_fixture(&intent, "intent_delete.json");
    /* only weigh carries grams and mode */
    CHECK(strstr(line, "\"grams\"") == NULL);
    CHECK(strstr(line, "\"mode\"") == NULL);
}

static void test_retry_resends_the_same_line(void) {
    /* A retry re-encodes the intent in the store: same id, same bytes, so
     * the host performs it at most once. */
    static char first[PROTO_LINE_MAX];
    static const IntentAction actions[] = {
        IntentActionFull, IntentActionEmpty, IntentActionDelete, IntentActionNew};
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 20);
    single_slot_intent_store_init(&slot);
    for(size_t a = 0; a < sizeof(actions) / sizeof(actions[0]); a++) {
        CHECK(intent_new_action(&intent, &ids, &mfc_scan, actions[a]));
        CHECK(intent_store_put(&slot.base, &intent));
        const Intent* current = intent_store_current(&slot.base);
        CHECK(current != NULL);
        if(!current) continue;
        size_t len = intent_encode(current, first, sizeof(first));
        CHECK(len > 0);
        CHECK_EQ_INT(intent_encode(current, line, sizeof(line)), len);
        CHECK_STR_EQ(line, first);
        CHECK_STR_EQ(current->id, intent.id);
    }
    CHECK_STR_EQ(intent.id, "i-000023");
}

static void test_net_mode_is_encoded(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 1);
    CHECK(intent_new_weigh(&intent, &ids, &ntag_scan, 0, WeightModeNet));
    CHECK(intent_encode(&intent, line, sizeof(line)) > 0);
    CHECK(strstr(line, "\"action\":\"weigh\",\"grams\":0,\"mode\":\"net\",\"tag\":{") != NULL);
}

static void test_ids_are_unique_per_call(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 1);
    CHECK(intent_new_weigh(&intent, &ids, &mfc_scan, 100, WeightModeGross));
    CHECK(intent_new_weigh(&other, &ids, &mfc_scan, 100, WeightModeGross));
    CHECK(strcmp(intent.id, other.id) != 0);
    CHECK_STR_EQ(intent.scan_id, other.scan_id);
    CHECK(intent_new_action(&other, &ids, &mfc_scan, IntentActionNew));
    CHECK_STR_EQ(other.id, "i-000003");
}

static void test_rejects_invalid_input(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 1);
    CHECK(!intent_new_weigh(&intent, &ids, &mfc_scan, PROTO_GRAMS_MAX + 1, WeightModeGross));
    CHECK(!intent_new_weigh(&intent, &ids, &mfc_scan, 10, (WeightMode)5));
    CHECK(!intent_new_action(&intent, &ids, &mfc_scan, IntentActionWeigh));
    CHECK(!intent_new_action(&intent, &ids, &mfc_scan, (IntentAction)99));
    intent.action = (IntentAction)99;
    CHECK_EQ_INT(intent_encode(&intent, line, sizeof(line)), 0);
    strcpy(mfc_scan.id, "bad id");
    CHECK(!intent_new_action(&intent, &ids, &mfc_scan, IntentActionNew));
    /* A rejected intent does not consume an id. */
    CHECK(intent_new_weigh(&intent, &ids, &ntag_scan, PROTO_GRAMS_MAX, WeightModeGross));
    CHECK_STR_EQ(intent.id, "i-000001");
}

static void test_encode_fails_on_small_buffer_or_bad_tag(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 3);
    CHECK(intent_new_weigh(&intent, &ids, &mfc_scan, 734, WeightModeGross));
    size_t full = intent_encode(&intent, line, sizeof(line));
    CHECK(full > 0);
    CHECK_EQ_INT(intent_encode(&intent, line, full), 0);
    CHECK_EQ_INT(intent_encode(&intent, line, full + 1), full);
    intent.tag.uid_len = 0;
    CHECK_EQ_INT(intent_encode(&intent, line, sizeof(line)), 0);
}

static void test_largest_intent_fits_in_a_line(void) {
    static const uint8_t page[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    static const uint8_t uid[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    static Scan big;
    IdSeq ids;
    tag_dump_init(&big.tag, TagTypeNtag);
    CHECK(tag_dump_set_uid(&big.tag, uid, sizeof(uid)));
    big.tag.sak = 255;
    for(int i = 0; i < TAG_DUMP_MAX_PAGES; i++)
        CHECK(tag_dump_append_page(&big.tag, page));
    memset(big.id, 'x', PROTO_ID_MAX);
    big.id[PROTO_ID_MAX] = '\0';
    id_seq_init(&ids, 'i', UINT32_MAX);
    CHECK(intent_new_weigh(&intent, &ids, &big, PROTO_GRAMS_MAX, WeightModeGross));
    size_t len = intent_encode(&intent, line, sizeof(line));
    CHECK(len > 0);
    CHECK(len + 1 <= PROTO_LINE_MAX);
}

static void test_single_slot_store_resolves_matching_result_only(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 3);
    single_slot_intent_store_init(&slot);
    IntentStore* store = &slot.base;
    CHECK(intent_store_current(store) == NULL);
    CHECK(!intent_store_resolve(store, "i-000003"));

    CHECK(intent_new_weigh(&intent, &ids, &mfc_scan, 734, WeightModeGross));
    CHECK(intent_store_put(store, &intent));
    const Intent* current = intent_store_current(store);
    CHECK(current != NULL);
    if(current) CHECK_STR_EQ(current->id, "i-000003");
    CHECK(current != &intent); /* the store keeps its own copy */

    CHECK(!intent_store_resolve(store, "i-000002"));
    CHECK(!intent_store_resolve(store, ""));
    CHECK(intent_store_current(store) != NULL);
    CHECK(intent_store_resolve(store, "i-000003"));
    CHECK(intent_store_current(store) == NULL);
    CHECK(!intent_store_resolve(store, "i-000003")); /* a repeated result is ignored */
}

static void test_single_slot_store_replaces_the_intent_in_flight(void) {
    IdSeq ids;
    load_scans();
    id_seq_init(&ids, 'i', 1);
    single_slot_intent_store_init(&slot);
    IntentStore* store = &slot.base;
    CHECK(intent_new_action(&intent, &ids, &mfc_scan, IntentActionNew));
    CHECK(intent_store_put(store, &intent));
    CHECK(intent_new_action(&other, &ids, &ntag_scan, IntentActionLinkOnHost));
    CHECK(intent_store_put(store, &other));
    CHECK(!intent_store_resolve(store, "i-000001")); /* superseded */
    const Intent* current = intent_store_current(store);
    CHECK(current != NULL);
    if(current) CHECK(current->action == IntentActionLinkOnHost);
    CHECK(intent_store_resolve(store, "i-000002"));
}

static void test_id_seed_from_time(void) {
    /* a thousand ids per second of the previous run before a reuse */
    CHECK_EQ_INT(id_seed_from_time(0), 0);
    CHECK_EQ_INT(id_seed_from_time(1), 1000);
    CHECK_EQ_INT(id_seed_from_time(1793000123), 123000);
    CHECK_EQ_INT(id_seed_from_time(1793999999), 999999000);
    CHECK_EQ_INT(id_seed_from_time(UINT32_MAX), 967295000);

    IdSeq seq;
    char id[PROTO_ID_MAX + 1];
    id_seq_init(&seq, 's', id_seed_from_time(1793999999));
    CHECK(id_seq_next(&seq, id));
    CHECK_STR_EQ(id, "s-999999000");
}

int main(void) {
    RUN_TEST(test_id_seq_formats_zero_padded_counter);
    RUN_TEST(test_weigh_matches_fixture);
    RUN_TEST(test_other_actions_match_fixtures);
    RUN_TEST(test_spool_actions_match_fixtures);
    RUN_TEST(test_retry_resends_the_same_line);
    RUN_TEST(test_net_mode_is_encoded);
    RUN_TEST(test_ids_are_unique_per_call);
    RUN_TEST(test_rejects_invalid_input);
    RUN_TEST(test_encode_fails_on_small_buffer_or_bad_tag);
    RUN_TEST(test_largest_intent_fits_in_a_line);
    RUN_TEST(test_single_slot_store_resolves_matching_result_only);
    RUN_TEST(test_single_slot_store_replaces_the_intent_in_flight);
    RUN_TEST(test_id_seed_from_time);
    return MINITEST_RESULT();
}
