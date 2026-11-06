#include "minitest.h"
#include "dump_loaders.h"

#include "bambu_colors.h"
#include "tag_facts.h"

static TagDump dump;
static TagFacts facts;

/* The synthetic fixture: PLA Basic Jade White, 1 kg (as the host's
 * TagParser and tools/bridge/bambu_tag.py read it). */
static void test_the_synthetic_bambu_tag(void) {
    load_mfc(&dump);
    CHECK(tag_facts_read(&dump, &facts));
    const ProtoTagInfo* i = &facts.info;
    CHECK_STR_EQ(i->material, "PLA");
    CHECK_STR_EQ(i->subtype, "Basic");
    CHECK(i->has_rgba);
    CHECK_EQ_INT(i->rgba, 0xFFFFFFFFu);
    CHECK(i->has_label);
    CHECK_EQ_INT(i->label, 1000);
    CHECK_STR_EQ(i->filament_id, "GFA00");
    CHECK_STR_EQ(i->code, "10100");
    CHECK_STR_EQ(i->color_name, "Jade White");
    CHECK_STR_EQ(i->diameter, "1.75");
    CHECK(i->has_nozzle_min);
    CHECK_EQ_INT(i->nozzle_min, 190);
    CHECK(i->has_nozzle_max);
    CHECK_EQ_INT(i->nozzle_max, 230);
    CHECK_EQ_INT(facts.drying_temp, 55);
    CHECK_EQ_INT(facts.drying_hours, 8);
    CHECK_EQ_INT(facts.bed_temp, 35);
}

/* The real Phase 0 dump: sectors 0-2 only; no bed temperature on it. */
static void test_the_real_bambu_tag(void) {
    load_mfc_real(&dump);
    CHECK(tag_facts_read(&dump, &facts));
    const ProtoTagInfo* i = &facts.info;
    CHECK_STR_EQ(i->material, "PLA");
    CHECK_STR_EQ(i->subtype, "Pure");
    CHECK_EQ_INT(i->rgba, 0xA4DBE8FFu);
    CHECK_EQ_INT(i->label, 1000);
    CHECK_STR_EQ(i->filament_id, "GFA19");
    CHECK_STR_EQ(i->code, "17600");
    CHECK_STR_EQ(i->color_name, "Baby Blue");
    CHECK_STR_EQ(i->diameter, "1.75");
    CHECK_EQ_INT(i->nozzle_min, 190);
    CHECK_EQ_INT(i->nozzle_max, 230);
    CHECK_EQ_INT(facts.drying_temp, 55);
    CHECK_EQ_INT(facts.drying_hours, 8);
    CHECK_EQ_INT(facts.bed_temp, 0);
}

static void test_an_ntag_carries_no_facts(void) {
    load_ntag(&dump);
    memset(&facts, 0xAA, sizeof(facts));
    CHECK(!tag_facts_read(&dump, &facts));
    CHECK_STR_EQ(facts.info.material, "");
    CHECK(!facts.info.has_rgba);
}

static void test_a_tag_without_its_data_sectors_carries_no_facts(void) {
    load_mfc(&dump);
    dump.block_present &= ~(uint64_t)0xFF; /* sectors 0 and 1 */
    CHECK(!tag_facts_read(&dump, &facts));
    /* a sector that did not read whole does not count, as on the host */
    load_mfc(&dump);
    dump.block_present &= ~((uint64_t)1 << 6);
    CHECK(tag_facts_read(&dump, &facts));
    CHECK_STR_EQ(facts.info.material, "PLA"); /* sector 0 */
    CHECK_STR_EQ(facts.info.filament_id, "GFA00");
    CHECK_STR_EQ(facts.info.subtype, ""); /* sector 1: blocks 4-6 */
    CHECK(!facts.info.has_rgba);
    CHECK(!facts.info.has_nozzle_max);
    CHECK_EQ_INT(facts.drying_temp, 0);
}

static void set_block(uint8_t block, const char* hex) {
    uint8_t data[TAG_DUMP_BLOCK_LEN];
    for(int i = 0; i < TAG_DUMP_BLOCK_LEN; i++) {
        unsigned v;
        sscanf(hex + 2 * i, "%2x", &v);
        data[i] = (uint8_t)v;
    }
    CHECK(tag_dump_set_block(&dump, block, data));
}

/* Only printable ASCII up to the NUL is text, trimmed, as on the host; zero
 * numbers and temperatures outside 1-999 are not on the tag. */
static void test_odd_values_are_left_out(void) {
    load_mfc(&dump);
    set_block(2, "504C41FF000000000000000000000000"); /* "PLA\xFF" */
    set_block(4, "20202000000000000000000000000000"); /* spaces */
    set_block(5, "11223344000000000000000000000000"); /* no label, no diameter */
    set_block(6, "E803000000000000E803000000000000"); /* 1000 C */
    CHECK(tag_facts_read(&dump, &facts));
    CHECK_STR_EQ(facts.info.material, "");
    CHECK_STR_EQ(facts.info.subtype, "");
    CHECK_EQ_INT(facts.info.rgba, 0x11223344u);
    CHECK(!facts.info.has_label);
    CHECK_STR_EQ(facts.info.diameter, "");
    CHECK(!facts.info.has_nozzle_min);
    CHECK(!facts.info.has_nozzle_max);
    CHECK_EQ_INT(facts.drying_temp, 0);
    CHECK_STR_EQ(facts.info.filament_id, "GFA00"); /* block 1 still says so */
    CHECK_STR_EQ(facts.info.code, "10100"); /* by its variant id, A00-W1 */
    set_block(1, "00000000000000004746413030000000"); /* no variant id */
    CHECK(tag_facts_read(&dump, &facts));
    CHECK_STR_EQ(facts.info.code, ""); /* and no row with RGB 112233 */
    CHECK_STR_EQ(facts.info.color_name, "");
}

/* 2.85 mm, and a detailed type that does not start with the material. */
static void test_diameter_and_subtype(void) {
    load_mfc(&dump);
    set_block(4, "50455447204846000000000000000000"); /* "PETG HF" */
    set_block(5, "000000FFE80300006666B64000000000"); /* 5.7 */
    CHECK(tag_facts_read(&dump, &facts));
    CHECK_STR_EQ(facts.info.subtype, "PETG HF");
    CHECK_STR_EQ(facts.info.diameter, "5.70");
    set_block(5, "000000FFE80300006666364000000000"); /* 2.85 */
    CHECK(tag_facts_read(&dump, &facts));
    CHECK_STR_EQ(facts.info.diameter, "2.85");
}

static void test_colours_by_variant_then_by_rgb(void) {
    BambuColor c;
    CHECK(bambu_color_find("GFA01", "A01-N3", false, 0, &c));
    CHECK_EQ_INT(c.code, 11803);
    CHECK_STR_EQ(c.name, "Caramel");
    CHECK(bambu_color_find("GFA01", "", true, 0xAE835BFFu, &c));
    CHECK_EQ_INT(c.code, 11803);
    CHECK(bambu_color_find("GFA19", "A19-B00", false, 0, &c)); /* a three-letter colour id */
    CHECK_STR_EQ(c.name, "Baby Blue");
    CHECK(!bambu_color_find("", "A01-N3", true, 0xAE835BFFu, &c));
    CHECK(!bambu_color_find("GFA01", "A01-ZZ", true, 0x123456FFu, &c));
    CHECK(!bambu_color_find("XXXXX", "A01-N3", false, 0, &c));
    CHECK(!bambu_color_find("GFA01", "A01-N", false, 0, &c)); /* not a prefix match */
}

int main(void) {
    RUN_TEST(test_the_synthetic_bambu_tag);
    RUN_TEST(test_the_real_bambu_tag);
    RUN_TEST(test_an_ntag_carries_no_facts);
    RUN_TEST(test_a_tag_without_its_data_sectors_carries_no_facts);
    RUN_TEST(test_odd_values_are_left_out);
    RUN_TEST(test_diameter_and_subtype);
    RUN_TEST(test_colours_by_variant_then_by_rgb);
    return MINITEST_RESULT();
}
