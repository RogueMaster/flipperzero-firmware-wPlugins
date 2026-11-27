"""Make a throwaway copy of the app that shows a made-up card (for screenshots)."""

import shutil, sys, os

src, dst = sys.argv[1], sys.argv[2]
if os.path.exists(dst):
    shutil.rmtree(dst)
shutil.copytree(
    src,
    dst,
    ignore=shutil.ignore_patterns(
        ".git", "dist", "tests", ".github", "tools", "screenshots"
    ),
)

p = os.path.join(dst, "cepas_app.c")
s = open(p, encoding="utf-8").read()

demo = r"""
/* DEMO ONLY: fake card for store screenshots. Not part of the real app. */
static void demo_txn(CepasTxn* t, uint8_t type, int32_t amount, uint32_t time, const char* ud) {
    t->type = type;
    t->amount = amount;
    t->time = time;
    snprintf(t->user_data, sizeof(t->user_data), "%s", ud);
}

static void demo_fill(App* app) {
    CepasCard* c = &app->card;
    memset(c, 0, sizeof(*c));
    static const uint8_t can[8] = {0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
    memcpy(c->purse.can, can, 8);
    c->purse.version = 2;
    c->purse.status = 1;
    c->purse.balance = 1230;
    c->purse.expiry_days = 12995; /* 31 Jul 2030 */
    c->has_purse = true;
    c->issuer = CepasIssuerEzLink;
    demo_txn(&c->txns[0], 48, -159, 974832120, "CTH-PGL");
    demo_txn(&c->txns[1], 49, -63, 974794500, "BUS118");
    demo_txn(&c->txns[2], 48, -99, 974746980, "SW5-SW6");
    demo_txn(&c->txns[3], 117, 2000, 974746500, "PGL GTM");
    demo_txn(&c->txns[4], 48, -170, 974655000, "AMK-PGL");
    demo_txn(&c->txns[5], 49, -112, 974620680, "BUS 851");
    demo_txn(&c->txns[6], 48, -149, 974571000, "LTI-PGL");
    demo_txn(&c->txns[7], 118, 21, 974534520, "BUS43");
    c->txn_count = 8;
    c->purse.log_count = 8;
    c->purse.last_txn = c->txns[0];
    c->purse.has_last_txn = true;
    app->state = AppStateResult;
    app->page = 0;
    app->hist_sel = 0;
}

int32_t cepas_app(void* p) {"""
assert "int32_t cepas_app(void* p) {" in s
s = s.replace("int32_t cepas_app(void* p) {", demo, 1)
old = "    app->nfc = nfc_alloc();\n    start_scan(app);"
assert old in s
s = s.replace(old, "    app->nfc = nfc_alloc();\n    demo_fill(app);")
# Never touch real cards in the demo: the scan screen shows, but no scanner runs.
old2 = "    nfc_scanner_start(app->scanner, scanner_callback, app);"
assert old2 in s
s = s.replace(old2, "    UNUSED(scanner_callback); /* demo: no real scanning */")
open(p, "w", encoding="utf-8").write(s)
print("demo copy ready at", dst)
