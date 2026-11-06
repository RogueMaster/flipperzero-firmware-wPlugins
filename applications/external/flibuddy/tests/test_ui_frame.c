#include "minitest.h"
#include "fixture.h"
#include "dump_loaders.h"

#include "ui_frame.h"
#include "ui_text.h"

#define FIXTURE_CAP 16384

static AppState app;
static SingleSlotIntentStore store;
static TagDump mfc;
static TagDump ntag;
static ProtoMessage msg;
static char text[FIXTURE_CAP];
static UiFrame frame;
static UiEnv env;

static AppEffect send_at(AppEventType type, uint32_t now_ms) {
    AppEvent e = {.type = type, .now_ms = now_ms};
    return app_reduce(&app, &e);
}

static AppEffect send(AppEventType type) {
    return send_at(type, 0);
}

static AppEffect press_at(AppKey key, uint32_t now_ms) {
    AppEvent e = {.type = AppEventKey, .now_ms = now_ms, .key = key};
    return app_reduce(&app, &e);
}

static AppEffect press(AppKey key) {
    return press_at(key, 0);
}

static void send_tag(const TagDump* tag) {
    CHECK(send(AppEventTagDetected) == AppEffectNone);
    AppEvent e = {.type = AppEventTagRead, .tag = tag};
    app_reduce(&app, &e);
}

static const ProtoMessage* fixture_message(const char* name) {
    char path[128];
    snprintf(path, sizeof(path), "fixtures/messages/%s", name);
    CHECK(fixture_read(path, text, sizeof(text)));
    CHECK(proto_decode_line(text, &msg) == ProtoDecodeOk);
    return &msg;
}

static void send_lookup(const ProtoLookup* lookup) {
    AppEvent e = {.type = AppEventLookupReceived, .lookup = lookup};
    app_reduce(&app, &e);
}

static void send_fixture_lookup(const char* name) {
    ProtoLookup l = fixture_message(name)->lookup;
    strcpy(l.scan_id, app.scan.id);
    send_lookup(&l);
}

static void send_fixture_result(const char* name) {
    ProtoResult r = fixture_message(name)->result;
    const Intent* in_flight = intent_store_current(&store.base);
    CHECK(in_flight != NULL);
    if(in_flight) strcpy(r.id, in_flight->id);
    AppEvent e = {.type = AppEventResultReceived, .result = &r};
    app_reduce(&app, &e);
}

/* The firmware fonts are ASCII only: every text a frame holds is printable
 * ASCII (the body's line breaks aside). */
static bool printable_ascii(const char* text, size_t cap, bool breaks) {
    for(size_t i = 0; i < cap && text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if(ch == '\n' && breaks) continue;
        if(ch < 0x20 || ch > 0x7E) return false;
    }
    return true;
}

#define ASCII(field) printable_ascii((field), sizeof(field), false)

static void check_frame_ascii(void) {
    CHECK(ASCII(frame.title));
    CHECK(ASCII(frame.note));
    CHECK(ASCII(frame.bar));
    CHECK(ASCII(frame.left));
    CHECK(ASCII(frame.center));
    CHECK(ASCII(frame.right));
    CHECK(printable_ascii(frame.body, sizeof(frame.body), true));
    for(int i = 0; i < UI_LINES; i++) {
        CHECK(ASCII(frame.line[i]));
        CHECK(ASCII(frame.key[i]));
    }
    for(int i = 0; i < UI_LIST_MAX; i++) {
        CHECK(ASCII(frame.item[i]));
        CHECK(ASCII(frame.value[i]));
    }
}

/* Every frame a test builds is checked for ASCII. */
static void build(void) {
    ui_frame_build(&frame, &app, &env);
    check_frame_ascii();
}

static void env_init(void) {
    env = (UiEnv){
        .now_ms = 0,
        .anim = 0,
        .version = "1.0",
        .firmware = "1.5.1",
        .device_name = "Workshop",
        .host_app = "",
    };
}

static void setup_no_host(void) {
    load_mfc(&mfc);
    load_ntag(&ntag);
    single_slot_intent_store_init(&store);
    app_state_init(&app, &store.base, 12, 3);
    env_init();
}

static void setup(void) {
    setup_no_host();
    send(AppEventHostConnected);
}

static void scan_found(void) {
    send_tag(&mfc);
    send_fixture_lookup("lookup_found.json");
    CHECK(app.screen == AppScreenResult);
}

static AppEffect choose_at(AppMenuItem item, uint32_t now_ms) {
    press_at(AppKeyLeft, now_ms);
    AppMenuItem items[APP_MENU_MAX];
    uint8_t n = app_menu_items(&app, items);
    for(uint8_t i = 0; i < n; i++) {
        if(items[app.sel] == item) return press_at(AppKeyOk, now_ms);
        press(AppKeyDown);
    }
    CHECK(!"menu item not offered");
    return AppEffectNone;
}

static AppEffect choose(AppMenuItem item) {
    return choose_at(item, 0);
}

/* FontSecondary is about 5 px a character; the draw code cuts with the
 * real widths (ui_text_fit). */
#define BESIDE_ICON_W 108 /* Waiting: right of the transport icon */
#define ABOUT_W       126

static uint16_t measure5(void* ctx, const char* text) {
    (void)ctx;
    return (uint16_t)(5 * strlen(text));
}

static bool fits(const char* text, uint16_t width) {
    return measure5(NULL, text) <= width;
}

static void check_chips(const char* left, const char* center, const char* right) {
    CHECK_STR_EQ(frame.left, left);
    CHECK_STR_EQ(frame.center, center);
    CHECK_STR_EQ(frame.right, right);
}

/* ---- Waiting, Ready ---- */

static void send_version(uint8_t version, TransportKind via) {
    AppEvent e = {.type = AppEventOtherVersion, .via = via, .version = version};
    app_reduce(&app, &e);
}

static void send_via(AppEventType type, TransportKind via) {
    AppEvent e = {.type = type, .via = via};
    app_reduce(&app, &e);
}

static void send_ble(AppBleStatus status) {
    AppEvent e = {.type = AppEventBleStatus, .ble = status};
    app_reduce(&app, &e);
}

/* Both transport lines always show, as in the design; the one not in use
 * says it is off. */
static void test_waiting_over_usb(void) {
    setup_no_host();
    app.connection = AppConnectionUsb;
    build();
    CHECK(frame.screen == AppScreenWaiting);
    CHECK_STR_EQ(frame.title, "FliBuddy");
    CHECK_STR_EQ(frame.note, "v1.0");
    CHECK(frame.icon == UiIconUsb);
    CHECK(frame.line_icon[0] == UiIconUsb);
    CHECK_STR_EQ(frame.line[0], "USB: no app");
    CHECK(frame.line_icon[1] == UiIconBt);
    CHECK_STR_EQ(frame.line[1], "Bluetooth off");
    CHECK_STR_EQ(frame.line[2], "Waiting for app");
    check_chips("", "Get app", "Settings");
    env.anim = 3;
    build();
    CHECK_STR_EQ(frame.line[2], "Waiting for app...");
    env.anim = 5;
    build();
    CHECK_STR_EQ(frame.line[2], "Waiting for app.");

    send(AppEventTransportFailed);
    build();
    CHECK_STR_EQ(frame.line[0], "USB unavailable");
}

static void test_waiting_names_the_protocol_of_another_host_app(void) {
    setup_no_host();
    send_version(3, TransportKindUsb);
    build();
    CHECK_STR_EQ(frame.line[0], "App protocol 3");
    CHECK(frame.line_icon[0] == UiIconUsb);
    CHECK_STR_EQ(frame.line[1], "Bluetooth off");
    CHECK_STR_EQ(frame.line[2], "Waiting for app");
    CHECK(fits(frame.line[0], BESIDE_ICON_W));

    send_version(12, TransportKindBluetooth);
    send_ble(AppBleConnected);
    build();
    CHECK_STR_EQ(frame.line[0], "App protocol 3");
    CHECK_STR_EQ(frame.line[1], "App protocol 12");
    CHECK(fits(frame.line[1], BESIDE_ICON_W));

    send_version(0, TransportKindUsb);
    build();
    CHECK_STR_EQ(frame.line[0], "USB: no app");
    CHECK_STR_EQ(frame.line[1], "App protocol 12");
}

static void test_waiting_over_bluetooth(void) {
    setup_no_host();
    app.connection = AppConnectionBluetooth;
    build();
    CHECK_STR_EQ(frame.line[0], "USB off");
    CHECK(frame.line_icon[0] == UiIconUsb);
    CHECK_STR_EQ(frame.line[1], "Bluetooth off");
    CHECK(frame.icon == UiIconBt);
    CHECK(frame.line_icon[1] == UiIconBt);

    send_ble(AppBleAdvertising);
    build();
    CHECK_STR_EQ(frame.line[1], "Visible as Workshop");
    CHECK(frame.icon == UiIconBt);
    CHECK(frame.line_icon[1] == UiIconBt);
    env.anim = 1; /* advertising without a host: the icon blinks */
    build();
    CHECK(frame.icon == UiIconNone);
    CHECK(frame.line_icon[1] == UiIconNone);
    CHECK(frame.line_icon[0] == UiIconUsb);

    send_ble(AppBleConnected);
    build();
    CHECK_STR_EQ(frame.line[1], "Bluetooth: no app");
    CHECK(frame.icon == UiIconBt);

    send_ble(AppBleAdvertising);
    send_via(AppEventTransportFailed, TransportKindBluetooth);
    build();
    CHECK_STR_EQ(frame.line[1], "Bluetooth off");
    CHECK(frame.line_icon[1] == UiIconBt); /* a failed one does not blink */
}

static void test_waiting_in_auto_shows_both_transports(void) {
    setup_no_host();
    CHECK(app.connection == AppConnectionAuto);
    send_ble(AppBleAdvertising);
    build();
    CHECK_STR_EQ(frame.line[0], "USB: no app");
    CHECK_STR_EQ(frame.line[1], "Visible as Workshop");
    CHECK(frame.icon == UiIconBt);
    env.anim = 1;
    build();
    CHECK(frame.icon == UiIconNone);
    CHECK(frame.line_icon[0] == UiIconUsb);
    CHECK(frame.line_icon[1] == UiIconNone);

    send_via(AppEventTransportFailed, TransportKindUsb);
    build();
    CHECK_STR_EQ(frame.line[0], "USB unavailable");
    CHECK_STR_EQ(frame.line[1], "Visible as Workshop");

    /* without Bluetooth the header shows USB */
    setup_no_host();
    send_via(AppEventTransportFailed, TransportKindBluetooth);
    build();
    CHECK(frame.icon == UiIconUsb);
    CHECK_STR_EQ(frame.line[0], "USB: no app");
    CHECK_STR_EQ(frame.line[1], "Bluetooth off");
}

static void test_ready(void) {
    setup();
    build();
    CHECK(frame.screen == AppScreenReady);
    CHECK_STR_EQ(frame.title, "FliBuddy");
    CHECK_STR_EQ(frame.bar, "");
    CHECK_STR_EQ(frame.line[0], "Ready");
    CHECK_STR_EQ(frame.line[1], "Hold spool tag");
    CHECK_STR_EQ(frame.line[2], "to the back");
    check_chips("Recent", "", "Settings");
    env.anim = 1;
    build();
    CHECK(frame.icon == UiIconUsb); /* the host's transport; only Bluetooth blinks */

    send(AppEventHostLost);
    send_via(AppEventHostConnected, TransportKindBluetooth);
    app.ble = AppBleAdvertising; /* still advertising: a host is connected */
    build();
    CHECK(frame.icon == UiIconBt);
}

static void test_ready_names_the_last_outcome(void) {
    setup();
    scan_found();
    press(AppKeyOk); /* Weight, pre-filled 734 g gross */
    press(AppKeyOk);
    send_fixture_result("result_ok.json");
    CHECK(app.screen == AppScreenSaved);
    build();
    CHECK_STR_EQ(frame.title, "Saved");
    CHECK_STR_EQ(frame.line[0], "PLA Basic - 484 g left");

    send_at(AppEventTick, APP_SAVED_MS);
    CHECK(app.screen == AppScreenReady);
    build();
    CHECK_STR_EQ(frame.bar, "Saved PLA Basic 484 g");
    CHECK_STR_EQ(frame.line[0], "Next spool");
    CHECK_STR_EQ(frame.line[1], "Hold tag to");
    CHECK_STR_EQ(frame.line[2], "the back");

    app.last = AppOutcomeAdded;
    build();
    CHECK_STR_EQ(frame.bar, "Added PLA Basic");
    app.last = AppOutcomeMarkedNew;
    build();
    CHECK_STR_EQ(frame.bar, "New PLA Basic 1000 g");
    app.last = AppOutcomeMarkedEmpty;
    build();
    CHECK_STR_EQ(frame.bar, "Empty PLA Basic");
    app.last = AppOutcomeDeleted;
    build();
    CHECK_STR_EQ(frame.bar, "Deleted PLA Basic");
}

/* ---- Reading, Looking up, Result ---- */

static void test_reading_and_looking_up(void) {
    setup();
    send(AppEventTagDetected);
    build();
    CHECK_STR_EQ(frame.title, "Reading tag");
    CHECK_STR_EQ(frame.line[0], "Keep still");

    AppEvent e = {.type = AppEventTagRead, .tag = &mfc};
    app_reduce(&app, &e);
    build();
    CHECK(frame.screen == AppScreenLookingUp);
    CHECK_STR_EQ(frame.title, "Looking up");
    CHECK_STR_EQ(frame.line[0], "PLA Basic"); /* block 4 of the tag */
    CHECK_STR_EQ(frame.line[1], "Asking the app");

    press(AppKeyBack);
    send_tag(&ntag);
    build();
    CHECK_STR_EQ(frame.line[0], "");
}

static void test_result_known(void) {
    setup();
    scan_found();
    build();
    CHECK(frame.kind == AppResultKnown);
    CHECK_STR_EQ(frame.title, "PLA Basic");
    CHECK(frame.mark == UiMarkSwatch);
    CHECK_EQ_INT(frame.lum, 255);
    CHECK_STR_EQ(frame.line[0], "Jade White");
    CHECK(frame.in_base);
    CHECK_STR_EQ(frame.line[1], "In Bambuddy #42");
    CHECK_STR_EQ(frame.line[2], "484 / 1000 g");
    CHECK_EQ_INT(frame.level, 484);
    CHECK_STR_EQ(frame.bar, "");
    check_chips("Menu", "Weigh", "New");

    /* never printed from: Unused in place of the level */
    app.spool.remaining = app.spool.label;
    build();
    CHECK_STR_EQ(frame.bar, "Unused - 1000 g");
    CHECK_EQ_INT(frame.level, -1);
    app.spool.remaining = 484;

    /* no colour name: the colour as hex */
    app.spool.color_name[0] = '\0';
    app.spool.rgba = 0x00AE42FF;
    build();
    CHECK_STR_EQ(frame.line[0], "Color #00AE42");
    CHECK_EQ_INT(frame.lum, 109);
    /* nor a colour: the tag type */
    app.spool.has_rgba = false;
    build();
    CHECK_STR_EQ(frame.line[0], "MIFARE Classic 1K");
    CHECK(frame.mark == UiMarkBlank);
}

static void test_result_known_without_spool_shows_host_lines(void) {
    setup();
    send_tag(&mfc);
    ProtoLookup l = fixture_message("lookup_found.json")->lookup;
    strcpy(l.scan_id, app.scan.id);
    l.has_spool = false;
    send_lookup(&l);
    CHECK(app.screen == AppScreenResult);
    build();
    CHECK(frame.kind == AppResultKnown);
    CHECK_STR_EQ(frame.title, "PLA Basic");
    CHECK_STR_EQ(frame.line[0], "Jade White");
    CHECK(!frame.in_base);
    CHECK_STR_EQ(frame.line[1], "Tag 5A3C9E21");
    CHECK_STR_EQ(frame.line[2], "484 g left of 1000");
    CHECK_EQ_INT(frame.level, -1);
    check_chips("Menu", "Weigh", "New");

    /* no lines either: the tag itself */
    l.lines.count = 0;
    send_lookup(&l);
    build();
    CHECK_STR_EQ(frame.line[0], "MIFARE Classic 1K");
    CHECK_STR_EQ(frame.line[2], "");

    /* the host dropped the empty material and subtype: the lines shift up and
     * are shown as they come, not taken for a name, a colour and a weight */
    l.lines.count = 3;
    strcpy(l.lines.line[0], "Jade White");
    strcpy(l.lines.line[1], "484 g left of 1000");
    strcpy(l.lines.line[2], "core 250 g");
    send_lookup(&l);
    build();
    CHECK_STR_EQ(frame.title, "Jade White");
    CHECK_STR_EQ(frame.line[0], "484 g left of 1000");
    CHECK_STR_EQ(frame.line[2], "core 250 g");
}

/* ▶ New on a known spool the host sent without its data: the toast has no
 * grams to name, so it leaves the line out rather than say "0 g left". */
static void test_toast_marked_new_names_grams_only_with_the_spool(void) {
    setup();
    send_tag(&mfc);
    ProtoLookup l = fixture_message("lookup_found.json")->lookup;
    strcpy(l.scan_id, app.scan.id);
    l.has_spool = false;
    send_lookup(&l);
    press(AppKeyRight);
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastMarkedNew);
    build();
    CHECK_STR_EQ(frame.title, "Marked as new");
    CHECK_STR_EQ(frame.line[1], "");
    CHECK_STR_EQ(frame.line[2], "Next spool in 3 s");
    check_chips("Undo", "Weigh", "");

    setup();
    scan_found();
    press(AppKeyRight);
    build();
    CHECK_STR_EQ(frame.title, "Marked as new");
    CHECK_STR_EQ(frame.line[1], "1000 g left - full");
}

static void test_result_archived(void) {
    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_found_archived.json");
    build();
    CHECK_STR_EQ(frame.bar, "Archived");
    CHECK_EQ_INT(frame.level, -1);
}

static void test_result_unknown(void) {
    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_unknown.json");
    build();
    CHECK(frame.kind == AppResultUnknownPrefill);
    CHECK_STR_EQ(frame.title, "PLA Basic");
    CHECK(frame.mark == UiMarkSwatch);
    CHECK_STR_EQ(frame.line[0], "Jade White");
    CHECK_STR_EQ(frame.line[1], "10100 - 1.75 mm - 1000 g");
    CHECK_STR_EQ(frame.line[2], "");
    CHECK_STR_EQ(frame.bar, "Not in Bambuddy");
    check_chips("Menu", "Add", "Link");
    /* without a colour name or specs: the hex colour and the UID */
    ProtoLookup l = fixture_message("lookup_unknown.json")->lookup;
    strcpy(l.scan_id, app.scan.id);
    l.tag_info.color_name[0] = '\0';
    l.tag_info.code[0] = '\0';
    l.tag_info.filament_id[0] = '\0';
    l.tag_info.diameter[0] = '\0';
    l.tag_info.has_label = false;
    l.tag_info.has_nozzle_min = l.tag_info.has_nozzle_max = false;
    send_lookup(&l);
    build();
    CHECK_STR_EQ(frame.line[0], "Color #FFFFFF");
    CHECK_STR_EQ(frame.line[1], "Tag 5A3C9E21");

    /* ▶ Link asks the host to link the tag */
    press(AppKeyRight);
    CHECK(app.screen == AppScreenLinkOnHost);
}

static void test_result_unknown_without_material(void) {
    setup();
    send_tag(&mfc);
    ProtoLookup l;
    memset(&l, 0, sizeof(l));
    strcpy(l.scan_id, app.scan.id);
    l.status = ProtoLookupUnknown;
    l.has_tag_info = true;
    send_lookup(&l);
    CHECK(app.result_kind == AppResultUnknownBare);
    build();
    CHECK_STR_EQ(frame.title, "Spool tag");
    CHECK(frame.mark == UiMarkBlank);
    CHECK_STR_EQ(frame.line[0], "MIFARE Classic 1K");
    CHECK_STR_EQ(frame.bar, "Not in Bambuddy");
    check_chips("Menu", "Create", "Link");
}

static void test_result_blank(void) {
    setup();
    send_tag(&ntag);
    send_fixture_lookup("lookup_unknown_blank.json");
    build();
    CHECK(frame.kind == AppResultBlank);
    CHECK_STR_EQ(frame.title, "Blank tag");
    CHECK(frame.mark == UiMarkBlank);
    CHECK_STR_EQ(frame.line[0], "NTAG215");
    CHECK_STR_EQ(frame.line[1], "Tag 0411...5566");
    CHECK_STR_EQ(frame.bar, "No spool data");
    check_chips("Menu", "Link", "");
}

/* Without the app a Bambu tag shows what it carries, as the host's
 * tag_info would: swatch, colour name, code - diameter - label. */
static void test_result_without_host(void) {
    setup_no_host();
    send_tag(&mfc);
    build();
    CHECK(frame.kind == AppResultNoHost);
    CHECK_STR_EQ(frame.title, "PLA Basic");
    CHECK(frame.mark == UiMarkSwatch);
    CHECK_EQ_INT(frame.lum, 255);
    CHECK_STR_EQ(frame.line[0], "Jade White");
    CHECK_STR_EQ(frame.line[1], "10100 - 1.75 mm - 1000 g");
    CHECK(fits(frame.line[1], 126));
    CHECK(!frame.in_base);
    CHECK_STR_EQ(frame.bar, "No app");
    check_chips("Menu", "", "");

    /* an NTAG carries nothing the app reads: its type and UID */
    press(AppKeyBack);
    send_tag(&ntag);
    build();
    CHECK_STR_EQ(frame.title, "NTAG215");
    CHECK(frame.mark == UiMarkBlank);
    CHECK_STR_EQ(frame.line[0], "NTAG215");
    CHECK_STR_EQ(frame.line[1], "Tag 0411...5566");

    /* the real dump: Bambu's name for its colour ids */
    press(AppKeyBack);
    TagDump real;
    load_mfc_real(&real);
    send_tag(&real);
    build();
    CHECK_STR_EQ(frame.title, "PLA Pure");
    CHECK_STR_EQ(frame.line[0], "Baby Blue");
    CHECK_STR_EQ(frame.line[1], "17600 - 1.75 mm - 1000 g");
}

/* A Bambu tag without its data sectors: its type and UID, as before. */
static void test_result_without_host_and_tag_data(void) {
    setup_no_host();
    TagDump bare = mfc;
    bare.block_present &= ~(uint64_t)0xFF;
    send_tag(&bare);
    build();
    CHECK(frame.mark == UiMarkBlank);
    CHECK_STR_EQ(frame.title, "MIFARE Classic 1K");
    CHECK_STR_EQ(frame.line[0], "MIFARE Classic 1K");
    CHECK_STR_EQ(frame.line[1], "Tag 5A3C9E21");
}

/* The tag's own facts never mix into what the host sent. */
static void test_with_the_host_only_its_answer_shows(void) {
    setup();
    send_tag(&mfc);
    ProtoLookup l = fixture_message("lookup_unknown_blank.json")->lookup;
    strcpy(l.scan_id, app.scan.id);
    send_lookup(&l);
    build();
    CHECK(frame.kind == AppResultBlank);
    CHECK(frame.mark == UiMarkBlank);
    CHECK_STR_EQ(frame.line[0], "MIFARE Classic 1K");
    CHECK_STR_EQ(frame.line[1], "Tag 5A3C9E21");
    choose(AppMenuDetails);
    build();
    for(int i = 0; i < 8; i++)
        press(AppKeyDown);
    build();
    CHECK_EQ_INT(frame.count, 3); /* Type, UID, Data */
    CHECK(!frame.more_down);
}

static void set_block_hex(TagDump* d, uint8_t block, const char* hex) {
    uint8_t data[TAG_DUMP_BLOCK_LEN];
    for(int i = 0; i < TAG_DUMP_BLOCK_LEN; i++) {
        unsigned v;
        sscanf(hex + 2 * i, "%2x", &v);
        data[i] = (uint8_t)v;
    }
    CHECK(tag_dump_set_block(d, block, data));
}

/* The no-app title follows tag_facts.h, the host's reading, not the raw
 * block text. */
static void test_the_no_app_title_is_the_tags_own_reading(void) {
    TagDump odd = mfc;
    odd.block_present &= ~((uint64_t)1 << 6); /* sector 1 incomplete: no block 4 */
    setup_no_host();
    send_tag(&odd);
    build();
    CHECK_STR_EQ(frame.title, "PLA");

    odd = mfc;
    set_block_hex(
        &odd,
        4,
        "504C4120426173"
        "FF6300000000000000"); /* "PLA Bas\xFFc" */
    setup_no_host();
    send_tag(&odd);
    build();
    CHECK_STR_EQ(frame.title, "PLA");

    odd = mfc;
    set_block_hex(&odd, 2, "504C41FF000000000000000000000000"); /* "PLA\xFF" */
    setup_no_host();
    send_tag(&odd);
    build();
    CHECK_STR_EQ(frame.title, "PLA Basic");
    press(AppKeyLeft);
    build();
    CHECK_STR_EQ(frame.title, "PLA Basic"); /* the menu's title too */
}

/* A spool shown with the host, then the host goes: the result and details
 * show the tag's own facts, not the spool's. */
static void test_a_lost_host_leaves_the_tags_own_facts(void) {
    setup();
    scan_found();
    CHECK(app.has_spool);
    send(AppEventHostLost);
    CHECK(app.result_kind == AppResultNoHost);
    CHECK(app.has_spool);
    build();
    CHECK_STR_EQ(frame.title, "PLA Basic");
    CHECK(!frame.in_base);
    CHECK_STR_EQ(frame.line[1], "10100 - 1.75 mm - 1000 g");
    CHECK_STR_EQ(frame.bar, "No app");
    choose(AppMenuDetails);
    build();
    CHECK_STR_EQ(frame.key[3], "Code");
    CHECK_STR_EQ(frame.line[3], "10100");
    for(int i = 0; i < 10; i++)
        press(AppKeyDown);
    build();
    CHECK_STR_EQ(frame.key[4], "Bed");
}

/* ▲ right after scrolling to the end moves at once: the state stops at the
 * last row, with the host's rows and with the tag's own. */
static void test_details_scroll_stops_at_the_last_row(void) {
    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_unknown.json");
    choose(AppMenuDetails);
    for(int i = 0; i < 8; i++)
        press(AppKeyDown);
    CHECK_EQ_INT(app.details_top, 4); /* 9 rows, 5 show */
    press(AppKeyUp);
    build();
    CHECK_EQ_INT(app.details_top, 3);
    CHECK_STR_EQ(frame.key[0], "Code");

    setup_no_host();
    send_tag(&mfc);
    choose(AppMenuDetails);
    for(int i = 0; i < 10; i++)
        press(AppKeyDown);
    CHECK_EQ_INT(app.details_top, 6); /* 11 rows */
    press(AppKeyUp);
    build();
    CHECK_STR_EQ(frame.key[0], "RGBA");

    setup();
    send_tag(&ntag);
    send_fixture_lookup("lookup_unknown_blank.json");
    choose(AppMenuDetails);
    press(AppKeyDown); /* 3 rows: nothing to scroll */
    CHECK_EQ_INT(app.details_top, 0);
    CHECK_EQ_INT(ui_details_count(&app), 3);
}

static void test_tag_details_without_host(void) {
    setup_no_host();
    send_tag(&mfc);
    choose(AppMenuDetails);
    build();
    CHECK_STR_EQ(frame.key[0], "Type");
    CHECK_STR_EQ(frame.key[3], "Code");
    CHECK_STR_EQ(frame.line[3], "10100");
    CHECK_STR_EQ(frame.key[4], "Fila");
    CHECK_STR_EQ(frame.line[4], "GFA00");
    CHECK(frame.more_down);
    for(int i = 0; i < 10; i++)
        press(AppKeyDown); /* past the end: clamped */
    build();
    CHECK(frame.more_up);
    CHECK(!frame.more_down);
    CHECK_EQ_INT(frame.count, 5);
    CHECK_STR_EQ(frame.key[0], "Label");
    CHECK_STR_EQ(frame.line[0], "1000 g");
    CHECK_STR_EQ(frame.key[1], "Diam");
    CHECK_STR_EQ(frame.line[1], "1.75 mm");
    CHECK_STR_EQ(frame.key[2], "Nozzle");
    CHECK_STR_EQ(frame.line[2], "190-230 C");
    CHECK_STR_EQ(frame.key[3], "Dry");
    CHECK_STR_EQ(frame.line[3], "55 C 8 h");
    CHECK_STR_EQ(frame.key[4], "Bed");
    CHECK_STR_EQ(frame.line[4], "35 C");
    for(int i = 0; i < 5; i++)
        CHECK(fits(frame.line[i], 90));
}

/* ---- Menu, Details, Delete ---- */

static void test_menu_lists_the_actions(void) {
    setup();
    scan_found();
    press(AppKeyLeft);
    build();
    CHECK_STR_EQ(frame.title, "PLA Basic");
    CHECK_EQ_INT(frame.count, 6);
    CHECK_STR_EQ(frame.item[0], "Weigh");
    CHECK_STR_EQ(frame.item[1], "Mark as new");
    CHECK_STR_EQ(frame.item[2], "Mark as empty");
    CHECK_STR_EQ(frame.item[3], "Relink in app");
    CHECK_STR_EQ(frame.item[4], "Tag details");
    CHECK_STR_EQ(frame.item[5], "Delete spool");
    CHECK_STR_EQ(frame.value[0], "");
    for(int i = 0; i < 5; i++)
        press(AppKeyDown);
    build();
    CHECK_EQ_INT(frame.sel, 5);
    CHECK_EQ_INT(frame.top, 2);

    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_unknown.json");
    press(AppKeyLeft);
    build();
    CHECK_EQ_INT(frame.count, 4);
    CHECK_STR_EQ(frame.item[0], "Add new");
    CHECK_STR_EQ(frame.item[1], "Add and weigh");
    CHECK_STR_EQ(frame.item[2], "Link in app");
    CHECK_STR_EQ(frame.item[3], "Tag details");

    app.result_kind = AppResultUnknownBare;
    build();
    CHECK_STR_EQ(frame.item[0], "Create in app");

    setup();
    send_tag(&ntag);
    send_fixture_lookup("lookup_unknown_blank.json");
    press(AppKeyLeft);
    build();
    CHECK_STR_EQ(frame.title, "Blank tag");
    CHECK_EQ_INT(frame.count, 2);
}

static void test_tag_details(void) {
    setup();
    scan_found();
    choose(AppMenuDetails);
    build();
    CHECK_STR_EQ(frame.title, "Tag details");
    CHECK_EQ_INT(frame.count, 5);
    CHECK_STR_EQ(frame.key[0], "Type");
    CHECK_STR_EQ(frame.line[0], "MIFARE Classic 1K");
    CHECK_STR_EQ(frame.key[1], "UID");
    CHECK_STR_EQ(frame.line[1], "5A:3C:9E:21");
    CHECK_STR_EQ(frame.key[2], "Data");
    CHECK_STR_EQ(frame.line[2], "15/16 sectors");
    CHECK_STR_EQ(frame.key[3], "RGBA");
    CHECK_STR_EQ(frame.line[3], "FFFFFFFF");
    CHECK_STR_EQ(frame.key[4], "Label");
    CHECK_STR_EQ(frame.line[4], "1000 g");
    CHECK(!frame.more_up);
    CHECK(!frame.more_down);

    /* a new tag has more rows than fit: ▲▼ scroll them */
    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_unknown.json");
    choose(AppMenuDetails);
    build();
    CHECK_EQ_INT(frame.count, 5);
    CHECK_STR_EQ(frame.key[3], "Code");
    CHECK_STR_EQ(frame.line[3], "10100");
    CHECK_STR_EQ(frame.key[4], "Fila");
    CHECK_STR_EQ(frame.line[4], "GFA00");
    CHECK(!frame.more_up);
    CHECK(frame.more_down);
    for(int i = 0; i < 8; i++)
        press(AppKeyDown); /* past the end: clamped */
    build();
    CHECK(frame.more_up);
    CHECK(!frame.more_down);
    CHECK_STR_EQ(frame.key[2], "Label");
    CHECK_STR_EQ(frame.key[3], "Diam");
    CHECK_STR_EQ(frame.line[3], "1.75 mm");
    CHECK_STR_EQ(frame.key[4], "Nozzle");
    CHECK_STR_EQ(frame.line[4], "190-230 C");
    press(AppKeyBack);
    CHECK(app.screen == AppScreenMenu);

    setup();
    send_tag(&ntag);
    send_fixture_lookup("lookup_unknown_blank.json");
    choose(AppMenuDetails);
    build();
    CHECK_EQ_INT(frame.count, 3);
    CHECK_STR_EQ(frame.line[0], "NTAG215");
    CHECK_STR_EQ(frame.line[1], "04:11:22:33:44:55:66");
    CHECK_STR_EQ(frame.line[2], "135 pages");
}

static void test_confirm_delete(void) {
    setup();
    scan_found();
    choose(AppMenuDelete);
    build();
    CHECK(frame.screen == AppScreenConfirmDelete);
    CHECK_STR_EQ(frame.title, "Delete spool?");
    CHECK_STR_EQ(frame.line[0], "PLA Basic Jade White");
    CHECK_STR_EQ(frame.line[1], "Removes it from");
    CHECK_STR_EQ(frame.line[2], "Bambuddy for good.");
    check_chips("Cancel", "", "Delete");
}

/* ---- Weight ---- */

static void test_weight(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    build();
    CHECK(frame.screen == AppScreenWeight);
    CHECK_STR_EQ(frame.title, "Weight");
    CHECK_EQ_INT(frame.digit[0], 0);
    CHECK_EQ_INT(frame.digit[1], 7);
    CHECK_EQ_INT(frame.digit[2], 3);
    CHECK_EQ_INT(frame.digit[3], 4);
    CHECK_EQ_INT(frame.cursor, 2);
    CHECK(!frame.net);
    CHECK(!frame.refused);
    CHECK_STR_EQ(frame.line[0], "Net 484 g - core 250");
    CHECK_STR_EQ(frame.bar, "");
    check_chips("", "Save", "");

    /* 100 g gross is below the 250 g core */
    press(AppKeyLeft); /* the first digit */
    press(AppKeyUp);
    press(AppKeyRight);
    for(int i = 0; i < 6; i++)
        press(AppKeyDown); /* 7 → 1 */
    press(AppKeyRight);
    for(int i = 0; i < 3; i++)
        press(AppKeyDown); /* 3 → 0 */
    press(AppKeyRight);
    for(int i = 0; i < 4; i++)
        press(AppKeyDown); /* 4 → 0 */
    press(AppKeyLeft);
    press(AppKeyLeft);
    press(AppKeyLeft); /* the first digit */
    press(AppKeyDown); /* 1 → 0 */
    CHECK_EQ_INT(app_weight_grams(&app), 100);
    build();
    CHECK(frame.refused);
    CHECK_STR_EQ(frame.bar, "Below core weight");
    CHECK_STR_EQ(frame.line[0], "");

    /* net: the toggle converts 100 g gross below the core to 0 g net */
    press(AppKeyLeft); /* the mode chip */
    press(AppKeyUp);
    build();
    CHECK_EQ_INT(frame.cursor, 0);
    CHECK(frame.net);
    CHECK(!frame.refused);
    CHECK_STR_EQ(frame.line[0], "Saves 0 g left");
}

static void test_weight_without_a_core(void) {
    setup();
    send_tag(&mfc);
    ProtoLookup l;
    memset(&l, 0, sizeof(l));
    strcpy(l.scan_id, app.scan.id);
    l.status = ProtoLookupUnknown;
    l.prefill = true;
    l.has_tag_info = true;
    strcpy(l.tag_info.material, "PLA");
    send_lookup(&l);
    choose(AppMenuAddAndWeigh);
    build();
    CHECK(frame.screen == AppScreenWeight);
    CHECK_STR_EQ(frame.line[0], "Core weight unknown");
}

/* ---- Busy, Saved, Toasts ---- */

static void test_busy_says_what_is_sent(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    build();
    CHECK(frame.screen == AppScreenBusy);
    CHECK_STR_EQ(frame.title, "Saving");
    CHECK_STR_EQ(frame.line[0], "PLA Basic - 484 g");
    CHECK_STR_EQ(frame.line[1], "Waiting for Bambuddy");

    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyLeft);
    press(AppKeyLeft);
    press(AppKeyUp); /* net */
    press(AppKeyOk);
    build();
    CHECK_STR_EQ(frame.line[0], "PLA Basic - 484 g");

    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_unknown.json");
    press(AppKeyOk);
    build();
    CHECK_STR_EQ(frame.title, "Adding");
    CHECK_STR_EQ(frame.line[0], "PLA Basic");

    setup();
    scan_found();
    choose(AppMenuDelete);
    press(AppKeyRight);
    build();
    CHECK_STR_EQ(frame.title, "Deleting");

    setup();
    scan_found();
    choose(AppMenuMarkNew);
    press(AppKeyBack); /* commits the mark */
    build();
    CHECK_STR_EQ(frame.title, "Updating");
}

static void test_toast_added(void) {
    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_created.json");
    CHECK(app.screen == AppScreenToast);
    build();
    CHECK(frame.toast == AppToastAdded);
    CHECK_STR_EQ(frame.title, "Added to Bambuddy");
    CHECK_STR_EQ(frame.line[0], "PLA Basic Jade White");
    CHECK_STR_EQ(frame.line[1], "Spool #31 - 1000 g");
    CHECK_STR_EQ(frame.line[2], "");
    check_chips("", "Weigh", "Next");
}

static void test_toast_marks_count_down(void) {
    setup();
    scan_found();
    choose_at(AppMenuMarkNew, 1000);
    CHECK(app.screen == AppScreenToast);
    env.now_ms = 1000;
    build();
    CHECK_STR_EQ(frame.title, "Marked as new");
    CHECK_STR_EQ(frame.line[0], "PLA Basic Jade White");
    CHECK_STR_EQ(frame.line[1], "1000 g left - full");
    CHECK_STR_EQ(frame.line[2], "Next spool in 3 s");
    CHECK_EQ_INT(frame.progress, UI_TOAST_MAX);
    check_chips("Undo", "Weigh", "");
    env.now_ms = 2500;
    build();
    CHECK_EQ_INT(frame.progress, 500);
    CHECK_STR_EQ(frame.line[2], "Next spool in 2 s");

    press(AppKeyLeft); /* Undo */
    choose(AppMenuMarkEmpty);
    env.now_ms = 0;
    build();
    CHECK_STR_EQ(frame.title, "Marked as empty");
    CHECK_STR_EQ(frame.line[1], "0 g left - archived");
    check_chips("Undo", "", "");
}

static void test_toast_deleted(void) {
    setup();
    scan_found();
    choose(AppMenuDelete);
    press_at(AppKeyRight, 0);
    send_fixture_result("result_delete.json");
    CHECK(app.screen == AppScreenToast);
    build();
    CHECK_STR_EQ(frame.title, "Deleted");
    CHECK_STR_EQ(frame.line[0], "PLA Basic Jade White");
    CHECK_STR_EQ(frame.line[1], "Tag is free to reuse");
    CHECK_STR_EQ(frame.line[2], "Next spool in 3 s");
    check_chips("", "", "");
}

/* ---- Link, Error ---- */

static void test_link_on_host(void) {
    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_unknown.json");
    choose(AppMenuLinkOnHost);
    build();
    CHECK(frame.screen == AppScreenLinkOnHost);
    CHECK_STR_EQ(frame.title, "Link tag");
    CHECK_STR_EQ(frame.line[0], "Pick the spool in");
    CHECK_STR_EQ(frame.line[1], "the FliBuddy app");
    check_chips("Cancel", "", "");

    app.result_kind = AppResultUnknownBare;
    press(AppKeyLeft);
    press(AppKeyOk);
    build();
    CHECK_STR_EQ(frame.title, "Create in app");
    CHECK_STR_EQ(frame.line[0], "Finish the spool in");
}

static void test_errors(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    send_fixture_result("result_error.json");
    build();
    CHECK(frame.screen == AppScreenError);
    CHECK_STR_EQ(frame.title, "Not saved");
    CHECK_STR_EQ(frame.body, "Not saved\nspool 42 not found");
    check_chips("Back", "", "Retry");

    press(AppKeyRight); /* resend */
    send_at(AppEventTick, APP_RESULT_TIMEOUT_MS);
    build();
    CHECK_STR_EQ(frame.title, "No app reply");
    CHECK_STR_EQ(frame.body, "No reply in 10 s. Open the FliBuddy app.");

    press(AppKeyRight);
    send(AppEventHostLost);
    build();
    CHECK_STR_EQ(frame.title, "App lost");
    CHECK_STR_EQ(frame.body, "The app went away before it answered.");
    check_chips("Back", "", ""); /* nothing to resend to */

    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_error.json");
    build();
    CHECK_STR_EQ(frame.title, "Lookup failed");
    CHECK_STR_EQ(frame.body, "Bambuddy unreachable\nconnection timed out");
    check_chips("Back", "", "");

    setup();
    send(AppEventTagDetected);
    send(AppEventReadFailed);
    build();
    CHECK_STR_EQ(frame.title, "Tag not read");
    CHECK_STR_EQ(frame.body, "Hold the tag still against the back. Reading again...");
    CHECK_EQ_INT(frame.progress, UI_TOAST_MAX);
    check_chips("", "", "");
}

static void test_unsupported_tag_screen_does_not_promise_a_retry(void) {
    setup();
    send(AppEventTagDetected);
    send(AppEventTagUnsupported);
    build();
    CHECK(frame.screen == AppScreenError);
    CHECK_STR_EQ(frame.title, "Unsupported tag");
    CHECK_STR_EQ(frame.body, "Not a spool tag FliBuddy can read.");
    CHECK(strstr(frame.body, "again") == NULL);
    CHECK_EQ_INT(frame.progress, UI_TOAST_MAX);
    check_chips("", "", "");
}

static void test_error_titles_follow_the_action(void) {
    setup();
    send_tag(&mfc);
    send_fixture_lookup("lookup_unknown.json");
    press(AppKeyOk); /* Add */
    send(AppEventSendFailed);
    build();
    CHECK_STR_EQ(frame.title, "Not sent");
    CHECK_STR_EQ(frame.body, "Could not send to the app.");

    ProtoResult r;
    memset(&r, 0, sizeof(r));
    press(AppKeyRight);
    strcpy(r.id, intent_store_current(&store.base)->id);
    AppEvent e = {.type = AppEventResultReceived, .result = &r};
    app_reduce(&app, &e);
    build();
    CHECK_STR_EQ(frame.title, "Not added");
    CHECK_STR_EQ(frame.body, "Bambuddy did not take the change.");

    app.held.action = IntentActionDelete;
    build();
    CHECK_STR_EQ(frame.title, "Not deleted");
    app.held.action = IntentActionFull;
    build();
    CHECK_STR_EQ(frame.title, "Not updated");
    app.held.action = IntentActionLinkOnHost;
    build();
    CHECK_STR_EQ(frame.title, "Not linked");

    app.error = AppErrorLookup;
    memset(&app.lookup, 0, sizeof(app.lookup));
    build();
    CHECK_STR_EQ(frame.body, "The app could not look the tag up.");
}

/* ---- Recent ---- */

static void save_weight_of(const TagDump* tag) {
    send_tag(tag);
    send_fixture_lookup("lookup_found.json");
    press(AppKeyOk);
    press(AppKeyOk);
    send_fixture_result("result_ok.json");
    press(AppKeyOk);
}

static void test_recent(void) {
    setup();
    press(AppKeyLeft);
    build();
    CHECK(frame.screen == AppScreenRecent);
    CHECK_STR_EQ(frame.title, "Recent");
    CHECK(frame.icon == UiIconUsb);
    CHECK_EQ_INT(frame.count, 0);
    CHECK_STR_EQ(frame.line[0], "No scans yet");
    check_chips("", "", "");
    press(AppKeyBack);

    TagDump other = mfc;
    other.uid[0] ^= 0xFF;
    save_weight_of(&mfc);
    save_weight_of(&other);
    press(AppKeyLeft);
    build();
    CHECK_STR_EQ(frame.line[0], "");
    CHECK_EQ_INT(frame.count, 2);
    CHECK(frame.checks);
    CHECK_STR_EQ(frame.item[0], "PLA Basic");
    CHECK_STR_EQ(frame.value[0], "484 g");
    CHECK(frame.fixed[0]); /* no arrows: nothing to change */
    CHECK_STR_EQ(frame.item[1], "PLA Basic");
    CHECK(frame.fixed[1]);
    CHECK_EQ_INT(frame.sel, 0);
    press(AppKeyDown);
    build();
    CHECK_EQ_INT(frame.sel, 1);
    press(AppKeyOk);
    build();
    CHECK(frame.screen == AppScreenResult);
    CHECK_STR_EQ(frame.title, "PLA Basic");
    CHECK_STR_EQ(frame.line[2], "484 / 1000 g");
    check_chips("Menu", "Weigh", "New");
}

/* ---- Settings, About ---- */

static void test_settings(void) {
    setup();
    press(AppKeyRight);
    build();
    CHECK_STR_EQ(frame.title, "Settings");
    CHECK_EQ_INT(frame.count, AppSettingCount);
    CHECK_STR_EQ(frame.item[0], "Connection");
    CHECK_STR_EQ(frame.value[0], "Auto");
    CHECK_STR_EQ(frame.item[1], "Auto-save new");
    CHECK_STR_EQ(frame.value[1], "Off");
    CHECK_STR_EQ(frame.item[2], "After scan");
    CHECK_STR_EQ(frame.value[2], "Show");
    CHECK_STR_EQ(frame.item[3], "Weigh as");
    CHECK_STR_EQ(frame.value[3], "Gross");
    CHECK_STR_EQ(frame.item[4], "Feedback");
    CHECK_STR_EQ(frame.value[4], "Vibro+LED");
    CHECK_STR_EQ(frame.item[5], "Sound");
    CHECK_STR_EQ(frame.value[5], "Off");
    CHECK_STR_EQ(frame.item[6], "Version");
    CHECK_STR_EQ(frame.value[6], "1.0");
    CHECK(!frame.fixed[0]);
    CHECK(!frame.fixed[5]);
    CHECK(frame.fixed[6]);

    press(AppKeyRight); /* USB */
    build();
    CHECK_STR_EQ(frame.value[0], "USB");
    press(AppKeyRight); /* Bluetooth */
    press(AppKeyDown);
    press(AppKeyRight);
    press(AppKeyDown);
    press(AppKeyRight); /* Weigh */
    press(AppKeyDown);
    press(AppKeyRight); /* Net */
    press(AppKeyDown);
    press(AppKeyRight); /* LED */
    press(AppKeyDown);
    press(AppKeyRight);
    build();
    CHECK_EQ_INT(frame.sel, 5);
    CHECK_EQ_INT(frame.top, 2);
    CHECK_STR_EQ(frame.value[0], "Bluetooth");
    CHECK_STR_EQ(frame.value[1], "On");
    CHECK_STR_EQ(frame.value[2], "Weigh");
    CHECK_STR_EQ(frame.value[3], "Net");
    CHECK_STR_EQ(frame.value[4], "LED");
    CHECK_STR_EQ(frame.value[5], "On");
    press(AppKeyUp);
    press(AppKeyRight);
    build();
    CHECK_STR_EQ(frame.value[4], "Off");
}

static void test_about(void) {
    setup();
    press(AppKeyRight);
    press(AppKeyUp); /* Version */
    press(AppKeyOk);
    env.host_app = "FliBuddy Mac";
    build();
    CHECK(frame.screen == AppScreenAbout);
    CHECK_STR_EQ(frame.title, "FliBuddy");
    CHECK_STR_EQ(frame.line[0], "for Bambuddy");
    CHECK_STR_EQ(frame.line[1], "Version 1.0");
    CHECK_STR_EQ(frame.line[2], "Protocol 2 - fw 1.5.1");
    CHECK_STR_EQ(frame.line[3], "USB app: FliBuddy Mac");

    send(AppEventHostLost);
    send_via(AppEventHostConnected, TransportKindBluetooth);
    build();
    CHECK(frame.screen == AppScreenAbout);
    CHECK_STR_EQ(frame.line[3], "BLE app: FliBuddy Mac");

    /* a long app is cut at its end: the transport stays in view */
    env.host_app = "flibuddy-watch/1.0.0";
    build();
    char shown[UI_TEXT_MAX];
    strcpy(shown, frame.line[3]);
    ui_text_fit(shown, ABOUT_W, measure5, NULL);
    CHECK(strncmp(shown, "BLE app: flibuddy", 17) == 0);
    send(AppEventHostLost);
    build();
    CHECK_STR_EQ(frame.line[3], "App: not connected");
}

/* FontPrimary is wider than measure5; the left column ends 5 px before the
 * QR code (x 67). */
#define GET_APP_TEXT_W 62

static void test_get_app(void) {
    setup_no_host();
    press(AppKeyOk);
    build();
    CHECK(frame.screen == AppScreenGetApp);
    CHECK_STR_EQ(frame.line[0], "No app yet?");
    CHECK_STR_EQ(frame.line[1], "Scan for the");
    CHECK_STR_EQ(frame.line[2], "iPhone app");
    CHECK_STR_EQ(frame.line[3], "App Store:");
    CHECK_STR_EQ(frame.line[4], "FliBuddy");
    CHECK_STR_EQ(frame.title, "");
    check_chips("Back", "", "");
    for(int i = 0; i < 5; i++)
        CHECK(fits(frame.line[i], GET_APP_TEXT_W));
}

/* Host texts reach the screen through the decoder, which turns every
 * non-ASCII character into '?'. */
static void test_host_texts_reach_the_screen_as_ascii(void) {
    setup();
    send_tag(&mfc);
    CHECK(fixture_read_compact("fixtures/messages/lookup_found.json", text, sizeof(text)));
    char* name = strstr(text, "\"Jade White\"");
    CHECK(name != NULL);
    if(name == NULL) return;
    /* "Jade White" -> "Jad\u00e9 Wh\u00df" as raw UTF-8, the same 10 bytes */
    memcpy(name + 1, "Jad\xc3\xa9 Wh\xc3\x9f", 10);
    CHECK(proto_decode_line(text, &msg) == ProtoDecodeOk);
    ProtoLookup l = msg.lookup;
    strcpy(l.scan_id, app.scan.id);
    send_lookup(&l);
    CHECK(app.screen == AppScreenResult);
    build();
    CHECK_STR_EQ(frame.line[0], "Jad? Wh?");
}

static void test_fields_of_another_screen_are_cleared(void) {
    setup();
    scan_found();
    build();
    CHECK_STR_EQ(frame.line[2], "484 / 1000 g");
    press(AppKeyBack);
    build();
    CHECK_STR_EQ(frame.left, "Recent"); /* Ready's, not the result's Menu */
    CHECK_STR_EQ(frame.center, "");
    CHECK_STR_EQ(frame.line[2], "to the back");
    CHECK(frame.count == 0);
}

int main(void) {
    RUN_TEST(test_waiting_over_usb);
    RUN_TEST(test_waiting_over_bluetooth);
    RUN_TEST(test_waiting_names_the_protocol_of_another_host_app);
    RUN_TEST(test_waiting_in_auto_shows_both_transports);
    RUN_TEST(test_unsupported_tag_screen_does_not_promise_a_retry);
    RUN_TEST(test_ready);
    RUN_TEST(test_ready_names_the_last_outcome);
    RUN_TEST(test_reading_and_looking_up);
    RUN_TEST(test_result_known);
    RUN_TEST(test_result_known_without_spool_shows_host_lines);
    RUN_TEST(test_toast_marked_new_names_grams_only_with_the_spool);
    RUN_TEST(test_result_archived);
    RUN_TEST(test_result_unknown);
    RUN_TEST(test_result_unknown_without_material);
    RUN_TEST(test_result_blank);
    RUN_TEST(test_result_without_host);
    RUN_TEST(test_menu_lists_the_actions);
    RUN_TEST(test_tag_details);
    RUN_TEST(test_result_without_host_and_tag_data);
    RUN_TEST(test_with_the_host_only_its_answer_shows);
    RUN_TEST(test_tag_details_without_host);
    RUN_TEST(test_the_no_app_title_is_the_tags_own_reading);
    RUN_TEST(test_a_lost_host_leaves_the_tags_own_facts);
    RUN_TEST(test_details_scroll_stops_at_the_last_row);
    RUN_TEST(test_confirm_delete);
    RUN_TEST(test_weight);
    RUN_TEST(test_weight_without_a_core);
    RUN_TEST(test_busy_says_what_is_sent);
    RUN_TEST(test_toast_added);
    RUN_TEST(test_toast_marks_count_down);
    RUN_TEST(test_toast_deleted);
    RUN_TEST(test_link_on_host);
    RUN_TEST(test_errors);
    RUN_TEST(test_error_titles_follow_the_action);
    RUN_TEST(test_recent);
    RUN_TEST(test_settings);
    RUN_TEST(test_about);
    RUN_TEST(test_get_app);
    RUN_TEST(test_host_texts_reach_the_screen_as_ascii);
    RUN_TEST(test_fields_of_another_screen_are_cleared);
    return MINITEST_RESULT();
}
