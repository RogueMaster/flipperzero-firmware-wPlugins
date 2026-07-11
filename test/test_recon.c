/* Host unit tests for the pure-logic modules. Compiled with a Furi shim (see
 * furi.h) and run in CI. No Flipper hardware or SDK required. */
#include "../models/session.h"
#include "../modules/asset_manager.h"
#include "../modules/graph_engine.h"

static int g_failures = 0;
static int g_checks = 0;
static uint32_t g_clock = 1000;

uint32_t furi_hal_rtc_get_timestamp(void) {
    return g_clock++;
}

#define CHECK(cond, msg)                                             \
    do {                                                             \
        g_checks++;                                                  \
        if(!(cond)) {                                                \
            printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            g_failures++;                                            \
        }                                                            \
    } while(0)

static void test_asset_crud(void) {
    printf("test_asset_crud\n");
    Session* s = session_alloc();

    uint16_t i0 = asset_manager_add(s);
    uint16_t i1 = asset_manager_add(s);
    CHECK(i0 == 0 && i1 == 1, "add returns sequential indices");
    CHECK(s->asset_count == 2, "asset_count is 2");
    CHECK(s->assets[0].id != s->assets[1].id, "assets have distinct ids");

    uint16_t id1 = s->assets[1].id;
    CHECK(asset_manager_index_by_id(s, id1) == 1, "index_by_id resolves");
    CHECK(asset_manager_index_by_id(s, 9999) == RECON_INVALID_INDEX, "unknown id -> invalid");

    CHECK(asset_manager_delete(s, 0), "delete first asset");
    CHECK(s->asset_count == 1, "asset_count is 1 after delete");
    CHECK(s->assets[0].id == id1, "remaining asset kept its id");

    session_free(s);
}

static void test_asset_capacity(void) {
    printf("test_asset_capacity\n");
    Session* s = session_alloc();
    for(int i = 0; i < RECON_MAX_ASSETS; i++) {
        CHECK(asset_manager_add(s) != RECON_INVALID_INDEX, "add within capacity");
    }
    CHECK(asset_manager_add(s) == RECON_INVALID_INDEX, "add past capacity is rejected");
    session_free(s);
}

static void test_evidence(void) {
    printf("test_evidence\n");
    Session* s = session_alloc();
    uint16_t a = asset_manager_add(s);
    uint16_t aid = s->assets[a].id;

    asset_manager_add_evidence(s, aid, EvidenceNfc, "badge", "/ext/nfc/b.nfc");
    asset_manager_add_evidence(s, aid, EvidenceRf, "gate", "/ext/subghz/g.sub");
    CHECK(asset_manager_evidence_count(s, aid) == 2, "two evidence items linked");

    /* deleting the asset must drop its evidence */
    asset_manager_delete(s, a);
    CHECK(s->evidence_count == 0, "evidence removed with owning asset");
    session_free(s);
}

static void test_relations(void) {
    printf("test_relations\n");
    Session* s = session_alloc();
    asset_manager_add(s);
    asset_manager_add(s);
    uint16_t a = s->assets[0].id, b = s->assets[1].id;

    CHECK(graph_add_relation(s, a, a, RelControls) == RECON_INVALID_INDEX, "self-loop rejected");
    CHECK(
        graph_add_relation(s, a, 4242, RelControls) == RECON_INVALID_INDEX,
        "missing target rejected");
    CHECK(graph_add_relation(s, a, b, RelControls) != RECON_INVALID_INDEX, "valid relation added");
    CHECK(s->relation_count == 1, "relation_count is 1");

    uint16_t out[8];
    CHECK(graph_neighbors_out(s, a, out, 8) == 1 && out[0] == b, "neighbor is b");
    CHECK(graph_degree(s, b) == 1, "degree of b is 1");

    CHECK(graph_delete_relation(s, 0), "delete relation");
    CHECK(s->relation_count == 0, "relation_count is 0");
    session_free(s);
}

static void test_risk_propagation(void) {
    printf("test_risk_propagation\n");
    Session* s = session_alloc();
    asset_manager_add(s); /* reader */
    asset_manager_add(s); /* door */
    s->assets[0].risk = 60;
    s->assets[1].risk = 10;
    graph_add_relation(s, s->assets[0].id, s->assets[1].id, RelControls);

    uint8_t risk[RECON_MAX_ASSETS];
    graph_propagate_risk(s, risk);
    CHECK(risk[0] == 60, "source keeps its own risk");
    CHECK(risk[1] == 45, "downstream door inherits reader risk minus decay");

    /* an 'observes' edge does not carry risk */
    Session* s2 = session_alloc();
    asset_manager_add(s2);
    asset_manager_add(s2);
    s2->assets[0].risk = 60;
    s2->assets[1].risk = 10;
    graph_add_relation(s2, s2->assets[0].id, s2->assets[1].id, RelObserves);
    uint8_t risk2[RECON_MAX_ASSETS];
    graph_propagate_risk(s2, risk2);
    CHECK(risk2[1] == 10, "observe edge does not raise risk");

    session_free(s);
    session_free(s2);
}

int main(void) {
    test_asset_crud();
    test_asset_capacity();
    test_evidence();
    test_relations();
    test_risk_propagation();

    printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
