#include "minitest.h"
#include "fixture.h"
#include "dump_loaders.h"

#include "app_state.h"
#include "proto_decode.h"

#define FIXTURE_CAP 16384

static AppState app;
static SingleSlotIntentStore store;
static TagDump mfc;
static TagDump ntag;
static ProtoMessage msg;
static char text[FIXTURE_CAP];

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

static AppEffect tick(uint32_t now_ms) {
    return send_at(AppEventTick, now_ms);
}

static AppEffect send_tag(const TagDump* tag) {
    AppEvent e = {.type = AppEventTagRead, .tag = tag};
    return app_reduce(&app, &e);
}

static AppEffect send_lookup(const ProtoLookup* lookup) {
    AppEvent e = {.type = AppEventLookupReceived, .lookup = lookup};
    return app_reduce(&app, &e);
}

static AppEffect send_result(const ProtoResult* result) {
    AppEvent e = {.type = AppEventResultReceived, .result = result};
    return app_reduce(&app, &e);
}

static const ProtoMessage* fixture_message(const char* name) {
    char path[128];
    snprintf(path, sizeof(path), "fixtures/messages/%s", name);
    CHECK(fixture_read(path, text, sizeof(text)));
    CHECK(proto_decode_line(text, &msg) == ProtoDecodeOk);
    return &msg;
}

/* A fixture lookup answering the scan on screen. */
static ProtoLookup fixture_lookup(const char* name) {
    ProtoLookup l = fixture_message(name)->lookup;
    CHECK(msg.type == ProtoMsgLookup);
    strcpy(l.scan_id, app.scan.id);
    return l;
}

static AppEffect send_fixture_lookup(const char* name) {
    ProtoLookup l = fixture_lookup(name);
    return send_lookup(&l);
}

/* A fixture result answering the intent in flight. */
static ProtoResult fixture_result(const char* name) {
    ProtoResult r = fixture_message(name)->result;
    CHECK(msg.type == ProtoMsgResult);
    const Intent* in_flight = intent_store_current(&store.base);
    CHECK(in_flight != NULL);
    if(in_flight) strcpy(r.id, in_flight->id);
    return r;
}

static AppEffect send_fixture_result(const char* name) {
    ProtoResult r = fixture_result(name);
    return send_result(&r);
}

static ProtoLookup lookup_with(ProtoLookupStatus status) {
    ProtoLookup l;
    memset(&l, 0, sizeof(l));
    strcpy(l.scan_id, app.scan.id);
    l.status = status;
    if(status == ProtoLookupFound) l.spool_id = 7;
    return l;
}

static const Intent* in_flight(void) {
    return intent_store_current(&store.base);
}

static IntentAction in_flight_action(void) {
    const Intent* i = in_flight();
    CHECK(i != NULL);
    return i ? i->action : (IntentAction)-1;
}

static const char* in_flight_id(void) {
    const Intent* i = in_flight();
    CHECK(i != NULL);
    return i ? i->id : "";
}

static void set_digits(uint16_t grams) {
    app.weight.digit[0] = (uint8_t)(grams / 1000);
    app.weight.digit[1] = (uint8_t)(grams / 100 % 10);
    app.weight.digit[2] = (uint8_t)(grams / 10 % 10);
    app.weight.digit[3] = (uint8_t)(grams % 10);
    app.weight.touched = true;
}

/* Fresh app without a host (Waiting); the next scan id is s-000012 and the
 * next intent id i-000003, as in the fixtures. */
static void setup_no_host(void) {
    load_mfc(&mfc);
    load_ntag(&ntag);
    single_slot_intent_store_init(&store);
    app_state_init(&app, &store.base, 12, 3);
}

/* The same with a host connected (Ready). */
static void setup(void) {
    setup_no_host();
    send(AppEventHostConnected);
}

/* Ready → Reading → LookingUp with the MFC fixture tag. */
static void scan_mfc(void) {
    CHECK(send(AppEventTagDetected) == AppEffectNone);
    CHECK(app.screen == AppScreenReading);
    CHECK(send_tag(&mfc) == AppEffectSendScan);
    CHECK(app.screen == AppScreenLookingUp);
}

static void scan_found(void) {
    scan_mfc();
    send_fixture_lookup("lookup_found.json");
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
}

static void scan_unknown(void) {
    scan_mfc();
    send_fixture_lookup("lookup_unknown.json");
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultUnknownPrefill);
}

/* Opens the Menu and chooses `item`. */
static AppEffect choose_at(AppMenuItem item, uint32_t now_ms) {
    CHECK(press_at(AppKeyLeft, now_ms) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
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

/* ---- start, connection, scanning ---- */

static void test_init_waits_for_a_host(void) {
    single_slot_intent_store_init(&store);
    app_state_init(&app, &store.base, 1, 1);
    CHECK(app.screen == AppScreenWaiting);
    CHECK(!app.connected);
    CHECK(app.connection == AppConnectionAuto);
    CHECK(app.ble == AppBleOff);
    CHECK(app.mode == WeightModeGross);
    CHECK(!app_settings_autosave(&app.settings));
    CHECK(app_settings_feedback(&app.settings) == AppFeedbackVibroLed);
    CHECK(!app_settings_sound(&app.settings));
    CHECK(app_settings_after_scan(&app.settings) == AppAfterScanShow);
    CHECK(app_settings_weigh_as(&app.settings) == AppWeighAsGross);
    /* the first app ever connected: recorded, so Get app never opens by itself */
    CHECK(send(AppEventHostConnected) == AppEffectSaveSettings);
    CHECK(app_settings_first_run_done(&app.settings));
    CHECK(app.cue == AppCueConnected);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeNone);
    CHECK(send(AppEventHostLost) == AppEffectNone);
    CHECK(app.cue == AppCueNone);
    CHECK(!app.connected);
    CHECK(app.screen == AppScreenWaiting);
}

static void test_waiting_and_ready_keys(void) {
    single_slot_intent_store_init(&store);
    app_state_init(&app, &store.base, 1, 1);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenGetApp);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
    CHECK(press(AppKeyBack) == AppEffectExit);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenSettings);

    setup();
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(press(AppKeyBack) == AppEffectExit);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenSettings);
}

/* ---- Get app ---- */

static void test_get_app_keys(void) {
    setup_no_host();
    press(AppKeyOk);
    CHECK(app.screen == AppScreenGetApp);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(press(AppKeyUp) == AppEffectNone);
    CHECK(press(AppKeyDown) == AppEffectNone);
    CHECK(app.screen == AppScreenGetApp);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
    press(AppKeyOk);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
}

static void test_a_connecting_app_moves_get_app_on_to_ready(void) {
    setup_no_host();
    press(AppKeyOk);
    send(AppEventHostConnected);
    CHECK(app.connected);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.cue == AppCueConnected);
}

static void test_the_first_start_opens_get_app_once(void) {
    setup_no_host();
    CHECK(!app_settings_first_run_done(&app.settings));
    CHECK(send(AppEventStart) == AppEffectSaveSettings);
    CHECK(app.screen == AppScreenGetApp);
    CHECK(app_settings_first_run_done(&app.settings));
    CHECK(app.cue == AppCueNone);
    /* the next start, with the saved flag */
    AppSettings saved = app.settings;
    setup_no_host();
    app.settings = saved;
    CHECK(send(AppEventStart) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
}

static void test_start_after_an_app_connected_stays_on_waiting(void) {
    setup_no_host();
    app_settings_set_first_run_done(&app.settings);
    CHECK(send(AppEventStart) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
    /* only from Waiting: a start event on another screen changes nothing */
    setup();
    app.settings.first_run_done = 0;
    CHECK(send(AppEventStart) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
}

static void test_a_later_connection_saves_nothing(void) {
    setup_no_host();
    app_settings_set_first_run_done(&app.settings);
    CHECK(send(AppEventHostConnected) == AppEffectNone);
    CHECK(send(AppEventHostLost) == AppEffectNone);
    CHECK(send(AppEventHostConnected) == AppEffectNone);
}

static void test_a_tag_on_get_app_shows_its_facts(void) {
    setup_no_host();
    press(AppKeyOk);
    CHECK(app_reader_wanted(&app));
    CHECK(send(AppEventTagDetected) == AppEffectNone);
    CHECK(app.screen == AppScreenReading);
    CHECK(send_tag(&mfc) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultNoHost);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenWaiting);
}

static void test_scan_gets_a_new_id_and_a_copy_of_the_tag(void) {
    setup();
    send(AppEventTagDetected);
    CHECK(send_tag(&mfc) == AppEffectSendScan);
    CHECK(app.cue == AppCueRead);
    CHECK_STR_EQ(app.scan.id, "s-000012");
    CHECK_MEM_EQ(&app.scan.tag, &mfc, sizeof(mfc));
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    send(AppEventTagDetected);
    send_tag(&ntag);
    CHECK_STR_EQ(app.scan.id, "s-000013");
}

static void test_scan_carries_the_autosave_setting(void) {
    char line[PROTO_LINE_MAX];
    setup();
    scan_mfc();
    CHECK(app_encode_scan(&app, line, sizeof(line)) > 0);
    CHECK(strstr(line, "\"scan_id\":\"s-000012\",\"autosave\":false,") != NULL);

    setup();
    app_settings_set_autosave(&app.settings, true);
    scan_mfc();
    CHECK(app_encode_scan(&app, line, sizeof(line)) > 0);
    CHECK(strstr(line, "\"autosave\":true,") != NULL);
    CHECK_EQ_INT(app_encode_intent(&app, line, sizeof(line)), 0); /* none in flight */
}

static void test_read_failed_returns_to_ready_by_itself(void) {
    setup();
    send_at(AppEventTagDetected, 1000);
    send_at(AppEventReadFailed, 1000);
    CHECK(app.screen == AppScreenError);
    CHECK_EQ_INT(app_timer_left_ms(&app, 1000), APP_READ_RETRY_MS);
    CHECK(tick(1000 + APP_READ_RETRY_MS - 1) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(tick(1000 + APP_READ_RETRY_MS) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeNone);
    CHECK(app_reader_wanted(&app));
}

static void test_unsupported_tag_is_its_own_quiet_outcome(void) {
    setup();
    send_at(AppEventTagDetected, 1000);
    CHECK(send_at(AppEventTagUnsupported, 1000) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorUnsupportedTag);
    CHECK(app.cue == AppCueNone); /* a tag nobody can read is no failure */
    CHECK(!app.retryable);
    CHECK_EQ_INT(app_timer_left_ms(&app, 1000), APP_READ_RETRY_MS);
    CHECK(tick(1000 + APP_READ_RETRY_MS) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeNone);
    /* only while reading */
    CHECK(send(AppEventTagUnsupported) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.cue == AppCueNone);
}

static void test_forget_the_tag_when_the_result_is_left_unanswered(void) {
    /* Lookup failed: Back, and the spool still on the Flipper is read again */
    setup();
    scan_mfc();
    send_fixture_lookup("lookup_error.json");
    CHECK(!app.forget_tag);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.forget_tag);

    /* a scan taken without a host */
    setup();
    send(AppEventHostLost);
    send(AppEventTagDetected);
    send_tag(&ntag);
    CHECK(app.result_kind == AppResultNoHost);
    CHECK(!app.forget_tag); /* still on the result: the reader would loop */
    press(AppKeyBack);
    CHECK(app.forget_tag);

    /* a scan the host could not get */
    setup();
    scan_mfc();
    send(AppEventSendFailed);
    press(AppKeyBack);
    CHECK(app.forget_tag);

    /* an answered scan is not read again */
    setup();
    scan_found();
    press(AppKeyBack);
    CHECK(app.screen == AppScreenReady);
    CHECK(!app.forget_tag);
}

static AppEffect send_via(AppEventType type, TransportKind via) {
    AppEvent e = {.type = type, .via = via};
    return app_reduce(&app, &e);
}

static AppEffect send_version(uint8_t version, TransportKind via) {
    AppEvent e = {.type = AppEventOtherVersion, .via = via, .version = version};
    return app_reduce(&app, &e);
}

static void test_other_host_version_is_remembered_per_transport_until_a_host_connects(void) {
    setup_no_host();
    CHECK_EQ_INT(app.other_version[TransportKindUsb], 0);
    CHECK_EQ_INT(app.other_version[TransportKindBluetooth], 0);
    CHECK(send_version(3, TransportKindBluetooth) == AppEffectNone);
    CHECK_EQ_INT(app.other_version[TransportKindBluetooth], 3);
    CHECK_EQ_INT(app.other_version[TransportKindUsb], 0);
    CHECK(app.screen == AppScreenWaiting);
    CHECK(app.cue == AppCueNone);
    send_version(4, TransportKindUsb);
    send(AppEventHostConnected);
    CHECK_EQ_INT(app.other_version[TransportKindUsb], 0);
    CHECK_EQ_INT(app.other_version[TransportKindBluetooth], 0);
    /* a connected host's stray hello says nothing */
    send_version(3, TransportKindBluetooth);
    CHECK_EQ_INT(app.other_version[TransportKindBluetooth], 0);
    /* the port closing clears it (version 0), on its transport only */
    send(AppEventHostLost);
    send_version(3, TransportKindBluetooth);
    send_version(5, TransportKindUsb);
    send_version(0, TransportKindUsb);
    CHECK_EQ_INT(app.other_version[TransportKindUsb], 0);
    CHECK_EQ_INT(app.other_version[TransportKindBluetooth], 3);
}

static void test_a_connected_host_names_its_transport(void) {
    setup_no_host();
    send_via(AppEventHostConnected, TransportKindBluetooth);
    CHECK(app.connected);
    CHECK(app.via == TransportKindBluetooth);
    send(AppEventHostLost);
    send_via(AppEventHostConnected, TransportKindUsb);
    CHECK(app.via == TransportKindUsb);
}

/* ▶ on a Bambu tag not in Bambuddy links it on the host; on a known spool
 * it marks the spool as new, with the toast's Undo. */
static void test_unknown_right_links_and_known_right_marks_new(void) {
    setup();
    scan_unknown();
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenLinkOnHost);
    CHECK(in_flight_action() == IntentActionLinkOnHost);

    setup();
    scan_found();
    CHECK(press_at(AppKeyRight, 1000) == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastMarkedNew);
    CHECK(app.cue == AppCueSuccess);
    CHECK(in_flight() == NULL);
    CHECK(press(AppKeyLeft) == AppEffectNone); /* Undo */
    CHECK(app.screen == AppScreenResult);
    CHECK(in_flight() == NULL);
    CHECK(press_at(AppKeyRight, 2000) == AppEffectNone);
    CHECK(tick(2000 + APP_TOAST_MS) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionFull);

    /* without a host ▶ does nothing */
    setup_no_host();
    CHECK(send(AppEventTagDetected) == AppEffectNone);
    CHECK(send_tag(&mfc) == AppEffectNone);
    CHECK(app.result_kind == AppResultNoHost);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
}

static void test_read_failed_and_back_while_reading(void) {
    setup();
    send(AppEventTagDetected);
    CHECK(send(AppEventReadFailed) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorReadFailed);
    CHECK(app.cue == AppCueError);
    CHECK(!app.retryable);
    CHECK(press(AppKeyRight) == AppEffectNone); /* nothing to retry */
    CHECK(app.screen == AppScreenReady);
    send(AppEventTagDetected);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    /* tag events outside Reading are ignored */
    CHECK(send_tag(&mfc) == AppEffectNone);
    CHECK(send(AppEventReadFailed) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
}

static void test_looking_up_back_and_stale_lookups(void) {
    setup();
    scan_mfc();
    ProtoLookup other = lookup_with(ProtoLookupFound);
    strcpy(other.scan_id, "s-000011");
    CHECK(send_lookup(&other) == AppEffectNone);
    CHECK(app.screen == AppScreenLookingUp);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenLookingUp);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    /* a late lookup for the abandoned scan */
    ProtoLookup late = lookup_with(ProtoLookupFound);
    CHECK(send_lookup(&late) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
}

static void test_no_host_scan_shows_local_facts(void) {
    setup();
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenWaiting);
    send(AppEventTagDetected);
    CHECK(send_tag(&ntag) == AppEffectNone);
    CHECK(app.cue == AppCueRead);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultNoHost);
    CHECK_MEM_EQ(&app.scan.tag, &ntag, sizeof(ntag));
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(in_flight() == NULL);
    AppMenuItem items[APP_MENU_MAX];
    CHECK_EQ_INT(app_menu_items(&app, items), 1);
    CHECK(items[0] == AppMenuDetails);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenDetails);
    press(AppKeyBack);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenResult);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
}

static void test_a_scan_that_cannot_be_sent_shows_local_facts(void) {
    setup();
    scan_mfc();
    CHECK(send(AppEventSendFailed) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultNoHost);
    /* the link is still up: the next scan tries the host again */
    CHECK(app.connected);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenReady);
    scan_mfc();
}

static void test_lookup_error_is_not_retryable(void) {
    setup();
    scan_mfc();
    send_fixture_lookup("lookup_error.json");
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorLookup);
    CHECK(app.cue == AppCueError);
    CHECK(!app.retryable);
    CHECK_STR_EQ(app.lookup.lines.line[0], "Bambuddy unreachable");
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
}

static void test_reader_runs_where_a_tag_is_welcome(void) {
    setup();
    CHECK(app_reader_wanted(&app)); /* Ready */
    send(AppEventTagDetected);
    CHECK(app_reader_wanted(&app)); /* Reading */
    send_tag(&mfc);
    CHECK(!app_reader_wanted(&app)); /* LookingUp */
    send_fixture_lookup("lookup_found.json");
    CHECK(app_reader_wanted(&app)); /* Result */
    press(AppKeyLeft);
    CHECK(!app_reader_wanted(&app)); /* Menu */
    press(AppKeyBack);
    press(AppKeyOk);
    CHECK(!app_reader_wanted(&app)); /* Weight */
    press(AppKeyOk);
    CHECK(!app_reader_wanted(&app)); /* Busy */
    send_fixture_result("result_ok.json");
    CHECK(app_reader_wanted(&app)); /* Saved */
    press(AppKeyOk);
    press(AppKeyRight);
    CHECK(app.screen == AppScreenSettings);
    CHECK(!app_reader_wanted(&app));
    send(AppEventHostLost);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenWaiting);
    CHECK(app_reader_wanted(&app));
}

static void test_a_tag_on_a_result_rescans_but_not_on_the_menu(void) {
    setup();
    scan_found();
    press(AppKeyLeft);
    CHECK(send(AppEventTagDetected) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    press(AppKeyBack);
    CHECK(send(AppEventTagDetected) == AppEffectNone);
    CHECK(app.screen == AppScreenReading);
    CHECK(send_tag(&ntag) == AppEffectSendScan);
    CHECK_STR_EQ(app.scan.id, "s-000013");
    CHECK(!app.has_spool);
}

/* ---- Result · known, Weight, Saved ---- */

static void test_found_weigh_saved_then_ready(void) {
    setup();
    scan_found();
    CHECK(app.has_spool);
    CHECK_EQ_INT(app.spool.id, 42);
    CHECK_EQ_INT(app.spool.remaining, 484);

    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
    CHECK_EQ_INT(app_weight_grams(&app), 734); /* 484 + core 250 */
    CHECK_EQ_INT(app.weight.cursor, 2);
    CHECK(!app.weight.touched);

    CHECK(press_at(AppKeyOk, 100) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenBusy);
    const Intent* i = in_flight();
    CHECK(i != NULL);
    if(i) {
        CHECK_STR_EQ(i->id, "i-000003");
        CHECK_STR_EQ(i->scan_id, "s-000012");
        CHECK(i->action == IntentActionWeigh);
        CHECK_EQ_INT(i->grams, 734);
        CHECK(i->mode == WeightModeGross);
        CHECK_MEM_EQ(&i->tag, &mfc, sizeof(mfc));
    }
    char line[PROTO_LINE_MAX];
    CHECK(app_encode_intent(&app, line, sizeof(line)) > 0);
    CHECK(strstr(line, "\"id\":\"i-000003\"") != NULL);

    CHECK(press(AppKeyBack) == AppEffectNone); /* Busy cannot be left */
    CHECK(app.screen == AppScreenBusy);

    ProtoResult ok = fixture_result("result_ok.json");
    ok.spool.remaining = 590;
    CHECK(send_result(&ok) == AppEffectNone);
    CHECK(app.screen == AppScreenSaved);
    CHECK(app.cue == AppCueSuccess);
    CHECK_EQ_INT(app.spool.remaining, 590);
    CHECK(in_flight() == NULL);
    CHECK(tick(APP_SAVED_MS - 1) == AppEffectNone);
    CHECK(app.screen == AppScreenSaved);
    CHECK_EQ_INT(app_timer_left_ms(&app, 300), APP_SAVED_MS - 300);
    CHECK(tick(APP_SAVED_MS) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeSaved);
    CHECK_EQ_INT(app_timer_left_ms(&app, APP_SAVED_MS), 0);
}

static void test_saved_ok_goes_to_ready_at_once(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    send_fixture_result("result_ok.json");
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeSaved);
    /* the next tag read clears the bar */
    scan_mfc();
    CHECK(app.last == AppOutcomeNone);
}

static void test_result_known_keys(void) {
    setup();
    scan_found();
    CHECK(press(AppKeyUp) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    CHECK_EQ_INT(app.sel, 0);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeNone);
}

static void test_archived_spool_is_known(void) {
    setup();
    scan_mfc();
    send_fixture_lookup("lookup_found_archived.json");
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
    CHECK(app.spool.archived);
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_grams(&app), 250); /* 0 left + core */
}

static void test_weight_digits_and_cursor(void) {
    setup();
    scan_found(); /* 0734 */
    press(AppKeyOk);
    CHECK(press(AppKeyUp) == AppEffectNone); /* cursor 2: hundreds */
    CHECK_EQ_INT(app_weight_grams(&app), 834);
    CHECK(app.weight.touched);
    press(AppKeyLeft);
    CHECK_EQ_INT(app.weight.cursor, 1);
    press(AppKeyDown); /* 0 wraps to 9 */
    CHECK_EQ_INT(app_weight_grams(&app), 9834);
    press(AppKeyUp); /* 9 wraps to 0 */
    CHECK_EQ_INT(app_weight_grams(&app), 834);
    press(AppKeyLeft);
    CHECK_EQ_INT(app.weight.cursor, 0); /* the mode chip */
    press(AppKeyLeft);
    CHECK_EQ_INT(app.weight.cursor, 4);
    press(AppKeyDown);
    CHECK_EQ_INT(app_weight_grams(&app), 833);
    press(AppKeyRight);
    CHECK_EQ_INT(app.weight.cursor, 0);
    press(AppKeyRight);
    CHECK_EQ_INT(app.weight.cursor, 1);
    CHECK(app.screen == AppScreenWeight);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(in_flight() == NULL);
}

static void test_mode_chip_converts_the_number(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyLeft);
    press(AppKeyLeft); /* cursor 0 */
    CHECK(press(AppKeyUp) == AppEffectNone);
    CHECK(app.mode == WeightModeNet);
    CHECK_EQ_INT(app_weight_grams(&app), 484);
    CHECK(!app.weight.touched);
    press(AppKeyDown);
    CHECK(app.mode == WeightModeGross);
    CHECK_EQ_INT(app_weight_grams(&app), 734);
    /* a typed number is converted too: gross - core, net + core */
    press(AppKeyRight);
    press(AppKeyUp); /* 1734 */
    press(AppKeyLeft);
    press(AppKeyUp);
    CHECK(app.mode == WeightModeNet);
    CHECK_EQ_INT(app_weight_grams(&app), 1484);
    press(AppKeyUp);
    CHECK(app.mode == WeightModeGross);
    CHECK_EQ_INT(app_weight_grams(&app), 1734);
    press(AppKeyUp);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight()->mode == WeightModeNet);
    CHECK_EQ_INT(in_flight()->grams, 1484);
}

static void test_mode_chip_floors_net_at_zero(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    /* the cursor opens on the hundreds */
    for(int i = 0; i < 7; i++)
        press(AppKeyDown); /* 734 -> 034, below the 250 g core */
    press(AppKeyLeft);
    press(AppKeyLeft); /* cursor 0 */
    press(AppKeyUp);
    CHECK(app.mode == WeightModeNet);
    CHECK_EQ_INT(app_weight_grams(&app), 0);
}

/* Weigh as picks the mode every Weight opens in; the chip changes it for
 * that entry only. */
static void test_weight_opens_in_the_weigh_as_mode(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    CHECK(app.mode == WeightModeGross);
    CHECK_EQ_INT(app_weight_grams(&app), 734);
    press(AppKeyLeft);
    press(AppKeyLeft);
    press(AppKeyUp); /* net for this entry */
    CHECK(app.mode == WeightModeNet);
    press(AppKeyBack);
    press(AppKeyOk);
    CHECK(app.mode == WeightModeGross);
    CHECK_EQ_INT(app_weight_grams(&app), 734);

    setup();
    app_settings_set_weigh_as(&app.settings, AppWeighAsNet);
    scan_found();
    press(AppKeyOk);
    CHECK(app.mode == WeightModeNet);
    CHECK_EQ_INT(app_weight_grams(&app), 484);
    CHECK_EQ_INT(app.weight.cursor, 2);
    press(AppKeyLeft);
    press(AppKeyLeft);
    press(AppKeyUp); /* gross for this entry */
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight()->mode == WeightModeGross);
    send_fixture_result("result_ok.json");
    press(AppKeyOk);
    scan_found();
    press(AppKeyOk);
    CHECK(app.mode == WeightModeNet);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight()->mode == WeightModeNet);
}

/* After scan = Weigh: a known spool's scan opens Weight; Back shows its
 * result. Anything else still shows the result. */
static void test_after_scan_weigh_opens_weight_for_a_known_spool(void) {
    setup();
    app_settings_set_after_scan(&app.settings, AppAfterScanWeigh);
    scan_mfc();
    CHECK(send_fixture_lookup("lookup_found.json") == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
    CHECK(app.result_kind == AppResultKnown);
    CHECK_EQ_INT(app.weight.cursor, 2);
    CHECK_EQ_INT(app_weight_grams(&app), 734);
    CHECK(app.cue == AppCueNone); /* the read already cued */
    press(AppKeyBack);
    CHECK(app.screen == AppScreenResult);

    /* with Weigh as Net, net */
    setup();
    app_settings_set_after_scan(&app.settings, AppAfterScanWeigh);
    app_settings_set_weigh_as(&app.settings, AppWeighAsNet);
    scan_mfc();
    send_fixture_lookup("lookup_found_archived.json");
    CHECK(app.screen == AppScreenWeight);
    CHECK(app.mode == WeightModeNet);

    /* an unknown tag, a blank one and an added spool are not weighed */
    setup();
    app_settings_set_after_scan(&app.settings, AppAfterScanWeigh);
    scan_mfc();
    send_fixture_lookup("lookup_unknown.json");
    CHECK(app.screen == AppScreenResult);
    setup();
    app_settings_set_after_scan(&app.settings, AppAfterScanWeigh);
    scan_mfc();
    send_fixture_lookup("lookup_unknown_blank.json");
    CHECK(app.screen == AppScreenResult);
    setup();
    app_settings_set_after_scan(&app.settings, AppAfterScanWeigh);
    app_settings_set_autosave(&app.settings, true);
    scan_mfc();
    send_fixture_lookup("lookup_created.json");
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastAdded);

    /* a fresh lookup for a result on screen (a link on the host) shows it */
    setup();
    app_settings_set_after_scan(&app.settings, AppAfterScanWeigh);
    scan_mfc();
    send_fixture_lookup("lookup_unknown.json");
    CHECK(press(AppKeyRight) == AppEffectSendIntent); /* Link */
    send_fixture_result("result_ok.json");
    send_fixture_lookup("lookup_found.json");
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);

    /* Show (the default) shows the result */
    setup();
    scan_found();
}

static void test_save_below_core_is_refused(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_core(&app), 250);
    set_digits(249);
    CHECK(app_weight_below_core(&app));
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.cue == AppCueError);
    CHECK(app.screen == AppScreenWeight);
    CHECK(in_flight() == NULL);
    set_digits(250);
    CHECK(!app_weight_below_core(&app));
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK_STR_EQ(in_flight_id(), "i-000003"); /* the refusal used no id */
}

static void test_net_or_unknown_core_never_refuses(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyLeft);
    press(AppKeyLeft);
    press(AppKeyUp); /* net */
    set_digits(0);
    CHECK(!app_weight_below_core(&app));
    CHECK(press(AppKeyOk) == AppEffectSendIntent);

    setup();
    scan_mfc();
    ProtoLookup l = lookup_with(ProtoLookupFound);
    send_lookup(&l);
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_core(&app), 0);
    CHECK(!app_weight_below_core(&app));
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK_EQ_INT(in_flight()->grams, 0);
}

static void test_weight_prefill_falls_back_to_the_defaults(void) {
    ProtoLookup l;
    /* no spool: label + core */
    setup();
    scan_mfc();
    l = lookup_with(ProtoLookupFound);
    l.defaults.has_label = true;
    l.defaults.label = 1000;
    l.defaults.has_core = true;
    l.defaults.core = 250;
    send_lookup(&l);
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_grams(&app), 1250);
    CHECK_EQ_INT(app_weight_core(&app), 250);

    /* the last gross weight first; net is gross - core, clamped at 0 */
    setup();
    scan_mfc();
    l.defaults.has_gross = true;
    l.defaults.gross = 200;
    send_lookup(&l);
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_grams(&app), 200);
    press(AppKeyLeft);
    press(AppKeyLeft);
    press(AppKeyUp);
    CHECK_EQ_INT(app_weight_grams(&app), 0);

    /* net without a gross: the label */
    setup();
    scan_mfc();
    l.defaults.has_gross = false;
    send_lookup(&l);
    app_settings_set_weigh_as(&app.settings, AppWeighAsNet);
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_grams(&app), 1000);

    /* capped to four digits */
    setup();
    scan_mfc();
    l.defaults.label = 9900;
    send_lookup(&l);
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_grams(&app), PROTO_GRAMS_MAX);

    /* nothing known */
    setup();
    scan_mfc();
    l = lookup_with(ProtoLookupFound);
    send_lookup(&l);
    press(AppKeyOk);
    CHECK_EQ_INT(app_weight_grams(&app), 0);
}

/* ---- Menu, Details, Delete ---- */

static void test_menu_items_per_kind(void) {
    AppMenuItem items[APP_MENU_MAX];
    setup();
    scan_found();
    CHECK_EQ_INT(app_menu_items(&app, items), 6);
    CHECK(items[0] == AppMenuWeigh);
    CHECK(items[1] == AppMenuMarkNew);
    CHECK(items[2] == AppMenuMarkEmpty);
    CHECK(items[3] == AppMenuRelink);
    CHECK(items[4] == AppMenuDetails);
    CHECK(items[5] == AppMenuDelete);

    setup();
    scan_unknown();
    CHECK_EQ_INT(app_menu_items(&app, items), 4);
    CHECK(items[0] == AppMenuAddNew);
    CHECK(items[1] == AppMenuAddAndWeigh);
    CHECK(items[2] == AppMenuLinkOnHost);
    CHECK(items[3] == AppMenuDetails);

    setup();
    scan_mfc();
    ProtoLookup bare = lookup_with(ProtoLookupUnknown);
    bare.has_tag_info = true;
    send_lookup(&bare);
    CHECK(app.result_kind == AppResultUnknownBare);
    CHECK_EQ_INT(app_menu_items(&app, items), 3);
    CHECK(items[0] == AppMenuCreateOnHost);
    CHECK(items[1] == AppMenuLinkOnHost);
    CHECK(items[2] == AppMenuDetails);

    setup();
    scan_mfc();
    send_fixture_lookup("lookup_unknown_blank.json");
    CHECK(app.result_kind == AppResultBlank);
    CHECK_EQ_INT(app_menu_items(&app, items), 2);
    CHECK(items[0] == AppMenuLinkOnHost);
    CHECK(items[1] == AppMenuDetails);
}

static void test_menu_selection_wraps_and_scrolls(void) {
    setup();
    scan_found();
    press(AppKeyLeft);
    for(int i = 0; i < 4; i++)
        press(AppKeyDown);
    CHECK_EQ_INT(app.sel, 4);
    CHECK_EQ_INT(app.top, 1);
    press(AppKeyDown);
    press(AppKeyDown); /* wraps */
    CHECK_EQ_INT(app.sel, 0);
    CHECK_EQ_INT(app.top, 0);
    press(AppKeyUp);
    CHECK_EQ_INT(app.sel, 5);
    CHECK_EQ_INT(app.top, 2);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    /* reopening starts at the top */
    press(AppKeyBack);
    press(AppKeyLeft);
    CHECK_EQ_INT(app.sel, 0);
    CHECK_EQ_INT(app.top, 0);
}

static void test_menu_weigh_and_details(void) {
    setup();
    scan_found();
    CHECK(choose(AppMenuDetails) == AppEffectNone);
    CHECK(app.screen == AppScreenDetails);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    press(AppKeyBack);
    choose(AppMenuDetails);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    press(AppKeyBack);
    CHECK(choose(AppMenuWeigh) == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
}

static void test_delete_only_from_the_confirmation(void) {
    setup();
    scan_found();
    CHECK(choose(AppMenuDelete) == AppEffectNone);
    CHECK(app.screen == AppScreenConfirmDelete);
    CHECK(press(AppKeyOk) == AppEffectNone); /* OK does nothing here */
    CHECK(press(AppKeyUp) == AppEffectNone);
    CHECK(app.screen == AppScreenConfirmDelete);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    press(AppKeyOk);
    CHECK(app.screen == AppScreenConfirmDelete);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenMenu);
    CHECK(in_flight() == NULL);
    press(AppKeyOk);
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenBusy);
    CHECK(in_flight_action() == IntentActionDelete);

    CHECK(send_fixture_result("result_delete.json") == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastDeleted);
    CHECK(app.cue == AppCueSuccess);
    CHECK(press(AppKeyLeft) == AppEffectNone); /* no Undo */
    CHECK(app.screen == AppScreenToast);
    CHECK(tick(APP_TOAST_MS - 1) == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    CHECK(tick(APP_TOAST_MS) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeDeleted);
    CHECK_STR_EQ(app.spool.material, "PLA"); /* named on the bar */
}

static void test_deleted_toast_back_clears_the_bar(void) {
    setup();
    scan_found();
    choose(AppMenuDelete);
    press(AppKeyRight);
    send_fixture_result("result_delete.json");
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeNone);
}

/* ---- Mark as new / empty with local Undo ---- */

static void test_mark_as_new_undo_sends_nothing(void) {
    setup();
    scan_found();
    CHECK(choose_at(AppMenuMarkNew, 1000) == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastMarkedNew);
    CHECK(app.cue == AppCueSuccess);
    CHECK(in_flight() == NULL);
    CHECK_EQ_INT(app_timer_left_ms(&app, 2000), APP_TOAST_MS - 1000);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
    CHECK(in_flight() == NULL);
    CHECK_EQ_INT(app.spool.remaining, 484);
    /* the dropped toast's deadline does nothing later */
    CHECK(tick(1000 + APP_TOAST_MS) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(in_flight() == NULL);
}

static void test_mark_as_new_commits_when_the_bar_runs_out(void) {
    setup();
    scan_found();
    choose_at(AppMenuMarkNew, 500);
    CHECK(tick(500 + APP_TOAST_MS - 1) == AppEffectNone);
    CHECK(in_flight() == NULL);
    CHECK(tick(500 + APP_TOAST_MS) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenBusy);
    CHECK(in_flight_action() == IntentActionFull);
    CHECK_STR_EQ(in_flight_id(), "i-000003");
    CHECK(tick(600 + APP_TOAST_MS) == AppEffectNone); /* sent once */
    CHECK(send_fixture_result("result_full.json") == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeMarkedNew);
    CHECK(app.cue == AppCueNone); /* cued on the toast */
    CHECK_EQ_INT(app.spool.remaining, 1000);
}

static void test_mark_as_new_ok_commits_then_weighs(void) {
    setup();
    scan_found();
    choose(AppMenuMarkNew);
    CHECK(app.toast == AppToastMarkedNew);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionFull);
    CHECK(app.screen == AppScreenBusy);
    CHECK(send_fixture_result("result_full.json") == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
    CHECK_EQ_INT(app_weight_grams(&app), 1250); /* the new remaining + core */
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionWeigh);
}

static void test_mark_as_new_back_commits(void) {
    setup();
    scan_found();
    choose(AppMenuMarkNew);
    CHECK(press(AppKeyBack) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionFull);
    send_fixture_result("result_full.json");
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeMarkedNew);
}

static void test_mark_as_empty_commits_on_a_new_tag(void) {
    setup();
    scan_found();
    CHECK(choose(AppMenuMarkEmpty) == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastMarkedEmpty);
    CHECK(in_flight() == NULL);
    CHECK(app_reader_wanted(&app));
    CHECK(send(AppEventTagDetected) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenBusy);
    CHECK(in_flight_action() == IntentActionEmpty);
    CHECK(!app_reader_wanted(&app));
    CHECK(send_fixture_result("result_empty.json") == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeMarkedEmpty);
    CHECK(app.spool.archived);
}

static void test_mark_as_empty_ok_and_undo(void) {
    setup();
    scan_found();
    choose(AppMenuMarkEmpty);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(press(AppKeyLeft) == AppEffectNone); /* Undo */
    CHECK(app.screen == AppScreenResult);
    CHECK(in_flight() == NULL);
    choose(AppMenuMarkEmpty);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionEmpty);
}

/* ---- unknown tags, Auto-save, Link on host ---- */

static void test_unknown_add_shows_added(void) {
    setup();
    scan_unknown();
    CHECK(!app.has_spool);
    CHECK(app.lookup.has_tag_info);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenBusy);
    CHECK(in_flight_action() == IntentActionNew);
    CHECK(send_fixture_result("result_new.json") == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastAdded);
    CHECK(app.cue == AppCueSuccess);
    CHECK(app.has_spool);
    CHECK_EQ_INT(app.spool.id, 31);
    CHECK(app.result_kind == AppResultKnown);
    /* no deadline */
    CHECK(tick(60000) == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    /* OK weighs the new spool; Back from Weight shows it known */
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
    CHECK_EQ_INT(app_weight_grams(&app), 1250);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
}

static void test_added_toast_next_and_back(void) {
    setup();
    scan_unknown();
    choose(AppMenuAddNew);
    CHECK(in_flight_action() == IntentActionNew);
    send_fixture_result("result_new.json");
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeAdded);

    setup();
    scan_unknown();
    press(AppKeyOk);
    send_fixture_result("result_new.json");
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.last == AppOutcomeNone);
}

static void test_autosave_lookup_created_goes_to_added(void) {
    setup();
    app_settings_set_autosave(&app.settings, true);
    scan_mfc();
    CHECK(send_fixture_lookup("lookup_created.json") == AppEffectNone);
    CHECK(app.screen == AppScreenToast);
    CHECK(app.toast == AppToastAdded);
    CHECK(app.cue == AppCueSuccess);
    CHECK(in_flight() == NULL);
    CHECK(app.result_kind == AppResultKnown);
    CHECK_EQ_INT(app.spool.id, 31);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
}

static void test_unknown_back_creates_nothing(void) {
    setup();
    scan_unknown();
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    CHECK(in_flight() == NULL);
}

static void test_add_and_weigh_sends_weigh_on_the_unknown_scan(void) {
    setup();
    scan_unknown();
    CHECK(choose(AppMenuAddAndWeigh) == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
    CHECK(in_flight() == NULL);
    CHECK_EQ_INT(app_weight_grams(&app), 1250); /* defaults label + core */
    CHECK_EQ_INT(app_weight_core(&app), 250);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionWeigh);
    CHECK_STR_EQ(in_flight()->scan_id, "s-000012");
    CHECK_EQ_INT(in_flight()->grams, 1250);
    CHECK(send_fixture_result("result_ok.json") == AppEffectNone);
    CHECK(app.screen == AppScreenSaved);
    CHECK(app.has_spool);
    CHECK(app.result_kind == AppResultKnown);
}

static void test_link_on_host_then_fresh_lookup(void) {
    setup();
    scan_unknown();
    CHECK(choose_at(AppMenuLinkOnHost, 100) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenLinkOnHost);
    CHECK(in_flight_action() == IntentActionLinkOnHost);
    ProtoResult ok = fixture_result("result_ok.json");
    ok.has_spool = false;
    CHECK(send_result(&ok) == AppEffectNone);
    CHECK(app.screen == AppScreenLinkOnHost);
    CHECK(app.cue == AppCueNone);
    /* no timeout once the host took it */
    CHECK(tick(100 + APP_RESULT_TIMEOUT_MS) == AppEffectNone);
    CHECK(app.screen == AppScreenLinkOnHost);
    CHECK(send_fixture_lookup("lookup_found.json") == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
    CHECK(app.cue == AppCueSuccess);
    CHECK_EQ_INT(app.spool.id, 42);
}

static void test_link_on_host_cancel_and_late_lookup(void) {
    setup();
    scan_mfc();
    send_fixture_lookup("lookup_unknown_blank.json");
    CHECK(app.result_kind == AppResultBlank);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionLinkOnHost);
    CHECK(app.screen == AppScreenLinkOnHost);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultBlank);
    /* the host links later: the result on screen is replaced */
    send_fixture_lookup("lookup_found.json");
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);

    setup();
    scan_unknown();
    choose(AppMenuLinkOnHost);
    CHECK(app.screen == AppScreenLinkOnHost);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    /* also on the menu and the details */
    choose(AppMenuDetails);
    send_fixture_lookup("lookup_found.json");
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
}

static void test_relink_and_create_on_host(void) {
    setup();
    scan_found();
    CHECK(choose(AppMenuRelink) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionLinkOnHost);
    CHECK(app.screen == AppScreenLinkOnHost);

    setup();
    scan_mfc();
    ProtoLookup bare = lookup_with(ProtoLookupUnknown);
    bare.has_tag_info = true;
    send_lookup(&bare);
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionCreateOnHost);
    CHECK(app.screen == AppScreenLinkOnHost);
    ProtoResult failed = fixture_result("result_error.json");
    CHECK(send_result(&failed) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.retryable);
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenLinkOnHost);
    CHECK(in_flight_action() == IntentActionCreateOnHost);

    /* ▶ links the tag on the host, and the menu offers Create on host too */
    setup();
    scan_mfc();
    send_lookup(&bare);
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionLinkOnHost);

    setup();
    scan_mfc();
    send_lookup(&bare);
    CHECK(choose(AppMenuCreateOnHost) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionCreateOnHost);
    CHECK(app.screen == AppScreenLinkOnHost);
}

static void test_stale_lookups_on_other_screens_are_ignored(void) {
    setup();
    scan_found();
    press(AppKeyOk); /* Weight */
    ProtoLookup unknown = fixture_lookup("lookup_unknown.json");
    CHECK(send_lookup(&unknown) == AppEffectNone);
    CHECK(app.screen == AppScreenWeight);
    press(AppKeyOk); /* Busy */
    CHECK(send_lookup(&unknown) == AppEffectNone);
    CHECK(app.screen == AppScreenBusy);
    send_fixture_result("result_ok.json"); /* Saved */
    CHECK(send_lookup(&unknown) == AppEffectNone);
    CHECK(app.screen == AppScreenSaved);
    CHECK(app.result_kind == AppResultKnown);

    /* an error lookup never replaces a result */
    setup();
    scan_found();
    ProtoLookup error = fixture_lookup("lookup_error.json");
    CHECK(send_lookup(&error) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
}

/* ---- results, errors, retry ---- */

static void test_results_not_in_flight_are_ignored(void) {
    setup();
    ProtoResult r = fixture_message("result_ok.json")->result;
    CHECK(send_result(&r) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    scan_found();
    CHECK(send_result(&r) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    press(AppKeyOk);
    press(AppKeyOk);
    strcpy(r.id, "i-000002");
    CHECK(send_result(&r) == AppEffectNone);
    CHECK(app.screen == AppScreenBusy);
    CHECK(in_flight() != NULL);
}

static void test_failed_intent_retries_with_the_same_id(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    CHECK(send_fixture_result("result_error.json") == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorHost);
    CHECK(app.cue == AppCueError);
    CHECK(app.retryable);
    CHECK_STR_EQ(app.result.error, "spool 42 not found");
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK(app.screen == AppScreenBusy);
    CHECK_STR_EQ(in_flight_id(), "i-000003");
    CHECK(in_flight_action() == IntentActionWeigh);
    CHECK_EQ_INT(in_flight()->grams, 734);
    send_fixture_result("result_ok.json");
    CHECK(app.screen == AppScreenSaved);
}

static void test_error_back_returns_to_the_result(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    send_fixture_result("result_error.json");
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);

    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    send_fixture_result("result_error.json");
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
}

static void test_timeout_then_retry(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    CHECK(press_at(AppKeyOk, 5000) == AppEffectSendIntent);
    CHECK(tick(5000 + APP_RESULT_TIMEOUT_MS - 1) == AppEffectNone);
    CHECK(app.screen == AppScreenBusy);
    CHECK(tick(5000 + APP_RESULT_TIMEOUT_MS) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorTimeout);
    CHECK(app.cue == AppCueError);
    CHECK(app.retryable);
    CHECK(press_at(AppKeyRight, 20000) == AppEffectSendIntent);
    CHECK_STR_EQ(in_flight_id(), "i-000003");
    CHECK(tick(20000 + APP_RESULT_TIMEOUT_MS - 1) == AppEffectNone);
    CHECK(app.screen == AppScreenBusy);
    /* a late answer to the first send still counts */
    send_fixture_result("result_ok.json");
    CHECK(app.screen == AppScreenSaved);
}

static void test_timeout_survives_the_tick_wrap(void) {
    setup();
    scan_unknown();
    CHECK(press_at(AppKeyOk, UINT32_MAX - 999) == AppEffectSendIntent);
    CHECK(tick(8999) == AppEffectNone); /* 9999 ms after, past the wrap */
    CHECK(app.screen == AppScreenBusy);
    CHECK(tick(9000) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorTimeout);
}

static void test_an_intent_that_cannot_be_sent_is_an_error(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    CHECK(send(AppEventSendFailed) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorSendFailed);
    CHECK(app.retryable);
    CHECK(tick(60000) == AppEffectNone);
    CHECK(app.error == AppErrorSendFailed);
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK_STR_EQ(in_flight_id(), "i-000003");
}

static void test_send_failed_elsewhere_is_ignored(void) {
    setup();
    CHECK(send(AppEventSendFailed) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    scan_found();
    CHECK(send(AppEventSendFailed) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
}

static void test_host_lost_mid_intent_retries_after_reconnect(void) {
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    CHECK(send(AppEventHostLost) == AppEffectNone);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorHostLost);
    CHECK(app.cue == AppCueError);
    CHECK(app.retryable);
    send(AppEventHostConnected);
    CHECK(app.screen == AppScreenError);
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK_STR_EQ(in_flight_id(), "i-000003");

    /* without a host, Retry and Back go to Waiting */
    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    send(AppEventHostLost);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);

    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    send(AppEventHostLost);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);
}

static void test_failed_commit_of_mark_as_new_retries_then_weighs(void) {
    setup();
    scan_found();
    choose(AppMenuMarkNew);
    press(AppKeyOk);
    send_fixture_result("result_error.json");
    CHECK(app.screen == AppScreenError);
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    CHECK(in_flight_action() == IntentActionFull);
    send_fixture_result("result_full.json");
    CHECK(app.screen == AppScreenWeight);
}

/* ---- host loss on every screen ---- */

static void check_host_lost_shows_no_host(void) {
    CHECK(send(AppEventHostLost) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultNoHost);
}

static void test_host_lost_on_screens_about_the_tag(void) {
    setup();
    scan_mfc(); /* LookingUp */
    check_host_lost_shows_no_host();

    setup();
    scan_found(); /* Result */
    check_host_lost_shows_no_host();
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);

    setup();
    scan_found();
    press(AppKeyLeft); /* Menu */
    check_host_lost_shows_no_host();

    setup();
    scan_found();
    choose(AppMenuDetails);
    check_host_lost_shows_no_host();

    setup();
    scan_found();
    choose(AppMenuDelete);
    check_host_lost_shows_no_host();

    setup();
    scan_found();
    press(AppKeyOk); /* Weight */
    check_host_lost_shows_no_host();

    setup();
    scan_unknown();
    press(AppKeyRight); /* LinkOnHost */
    check_host_lost_shows_no_host();

    /* an uncommitted mark cannot be sent: it is dropped */
    setup();
    scan_found();
    press(AppKeyRight);
    check_host_lost_shows_no_host();
    CHECK(in_flight() == NULL);
    CHECK(tick(APP_TOAST_MS) == AppEffectNone);
    CHECK(in_flight() == NULL);
}

static void test_host_lost_on_other_screens(void) {
    setup();
    send(AppEventTagDetected); /* Reading */
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenReading);
    CHECK(send_tag(&mfc) == AppEffectNone);
    CHECK(app.result_kind == AppResultNoHost);

    setup();
    scan_found();
    press(AppKeyOk);
    press(AppKeyOk);
    send_fixture_result("result_ok.json"); /* Saved */
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenSaved);
    tick(APP_SAVED_MS);
    CHECK(app.screen == AppScreenWaiting);

    setup();
    scan_unknown();
    press(AppKeyOk);
    send_fixture_result("result_new.json"); /* Toast · Added */
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenToast);
    CHECK(press(AppKeyOk) == AppEffectNone); /* no Weigh without a host */
    CHECK(app.screen == AppScreenToast);
    CHECK(press(AppKeyRight) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting);

    setup();
    scan_found();
    choose(AppMenuDelete);
    press(AppKeyRight);
    send_fixture_result("result_delete.json"); /* Toast · Deleted */
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenToast);
    tick(APP_TOAST_MS);
    CHECK(app.screen == AppScreenWaiting);

    setup();
    scan_mfc();
    send_fixture_lookup("lookup_error.json"); /* Error */
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenError);
    CHECK(app.error == AppErrorLookup);

    setup();
    press(AppKeyRight); /* Settings */
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenSettings);
    press(AppKeyUp);
    press(AppKeyOk); /* About */
    CHECK(app.screen == AppScreenAbout);
    send(AppEventHostConnected);
    CHECK(app.screen == AppScreenAbout);
}

/* ---- Recent ---- */

/* Scans `tag`, weighs it as pre-filled and saves; the host answers with
 * result_ok (484 g left). Ends on Ready. */
static void weigh_and_save(const TagDump* tag) {
    send(AppEventTagDetected);
    CHECK(send_tag(tag) == AppEffectSendScan);
    send_fixture_lookup("lookup_found.json");
    CHECK(press(AppKeyOk) == AppEffectNone); /* Weight */
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    send_fixture_result("result_ok.json");
    CHECK(app.screen == AppScreenSaved);
    press(AppKeyOk);
    CHECK(app.screen == AppScreenReady);
}

static TagDump with_uid(uint8_t first) {
    TagDump t = mfc;
    t.uid[0] = first;
    return t;
}

static void test_recent_opens_from_ready_and_starts_empty(void) {
    setup_no_host();
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenWaiting); /* no host, nothing to do with one */
    send(AppEventHostConnected);
    CHECK(press(AppKeyLeft) == AppEffectNone);
    CHECK(app.screen == AppScreenRecent);
    CHECK_EQ_INT(app.recent_count, 0);
    CHECK(!app_reader_wanted(&app));
    CHECK(press(AppKeyOk) == AppEffectNone);
    press(AppKeyDown);
    CHECK(app.screen == AppScreenRecent);
    CHECK_EQ_INT(app.sel, 0);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
}

static void test_a_saved_weight_goes_to_the_top_of_recent(void) {
    setup();
    TagDump a = with_uid(0xA1), b = with_uid(0xB2);
    weigh_and_save(&a);
    CHECK_EQ_INT(app.recent_count, 1);
    CHECK_EQ_INT(app.recent[0].scan.tag.uid[0], 0xA1);
    CHECK_STR_EQ(app.recent[0].scan.id, "s-000012");
    CHECK_EQ_INT(app.recent[0].spool.remaining, 484);
    CHECK_EQ_INT(app.recent[0].spool.id, 42);
    weigh_and_save(&b);
    CHECK_EQ_INT(app.recent_count, 2);
    CHECK_EQ_INT(app.recent[0].scan.tag.uid[0], 0xB2);
    CHECK_EQ_INT(app.recent[1].scan.tag.uid[0], 0xA1);
    /* the same tag again moves up, it is not listed twice */
    weigh_and_save(&a);
    CHECK_EQ_INT(app.recent_count, 2);
    CHECK_EQ_INT(app.recent[0].scan.tag.uid[0], 0xA1);
    CHECK_STR_EQ(app.recent[0].scan.id, "s-000014");
    CHECK_EQ_INT(app.recent[1].scan.tag.uid[0], 0xB2);
}

static void test_recent_keeps_the_last_six(void) {
    setup();
    for(uint8_t i = 1; i <= APP_RECENT_MAX + 1; i++) {
        TagDump t = with_uid(i);
        weigh_and_save(&t);
    }
    CHECK_EQ_INT(APP_RECENT_MAX, 6);
    CHECK_EQ_INT(app.recent_count, APP_RECENT_MAX);
    CHECK_EQ_INT(app.recent[0].scan.tag.uid[0], APP_RECENT_MAX + 1);
    CHECK_EQ_INT(app.recent[APP_RECENT_MAX - 1].scan.tag.uid[0], 2); /* the first went */
}

/* OK on an entry shows its result as if just scanned: weighing it again
 * needs no scan, and the intent names the entry's scan and tag. */
static void test_recent_entry_opens_its_result(void) {
    setup();
    TagDump a = with_uid(0xA1), b = with_uid(0xB2);
    weigh_and_save(&a);
    weigh_and_save(&b);
    press(AppKeyLeft);
    press(AppKeyDown);
    CHECK_EQ_INT(app.sel, 1);
    press(AppKeyDown); /* wraps */
    CHECK_EQ_INT(app.sel, 0);
    press(AppKeyUp);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenResult);
    CHECK(app.result_kind == AppResultKnown);
    CHECK(app.has_spool);
    CHECK_EQ_INT(app.spool.remaining, 484);
    CHECK_EQ_INT(app.scan.tag.uid[0], 0xA1);
    CHECK_STR_EQ(app.scan.id, "s-000012");
    CHECK(app.last == AppOutcomeNone);
    CHECK(press(AppKeyOk) == AppEffectNone); /* Weight */
    CHECK(app.screen == AppScreenWeight);
    CHECK_EQ_INT(app_weight_grams(&app), 734); /* 484 + the 250 g core */
    CHECK(press(AppKeyOk) == AppEffectSendIntent);
    CHECK_STR_EQ(in_flight()->scan_id, "s-000012");
    CHECK_EQ_INT(in_flight()->tag.uid[0], 0xA1);
    send_fixture_result("result_ok.json");
    press(AppKeyOk);
    press(AppKeyLeft);
    press(AppKeyOk);
    CHECK(app.screen == AppScreenResult);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenReady);
}

static void test_recent_follows_marks_and_deletes(void) {
    setup();
    TagDump a = with_uid(0xA1), b = with_uid(0xB2);
    weigh_and_save(&a);
    weigh_and_save(&b);
    /* B marked empty: its entry says so, in place */
    press(AppKeyLeft);
    press(AppKeyOk);
    CHECK_EQ_INT(app.scan.tag.uid[0], 0xB2);
    choose(AppMenuMarkEmpty);
    CHECK(press(AppKeyBack) == AppEffectSendIntent); /* commits */
    send_fixture_result("result_empty.json");
    CHECK_EQ_INT(app.recent_count, 2);
    CHECK_EQ_INT(app.recent[0].scan.tag.uid[0], 0xB2);
    CHECK_EQ_INT(app.recent[0].spool.remaining, 0);
    CHECK(app.recent[0].spool.archived);
    /* a later scan of A refreshes its entry */
    send(AppEventTagDetected);
    send_tag(&a);
    ProtoLookup l = fixture_lookup("lookup_found.json");
    l.spool.remaining = 300;
    send_lookup(&l);
    CHECK_EQ_INT(app.recent[1].spool.remaining, 300);
    CHECK_STR_EQ(app.recent[1].scan.id, app.scan.id);
    /* A deleted: its entry goes */
    choose(AppMenuDelete);
    CHECK(press(AppKeyRight) == AppEffectSendIntent);
    send_fixture_result("result_delete.json");
    CHECK_EQ_INT(app.recent_count, 1);
    CHECK_EQ_INT(app.recent[0].scan.tag.uid[0], 0xB2);
}

static void test_losing_the_host_leaves_recent(void) {
    setup();
    weigh_and_save(&mfc);
    press(AppKeyLeft);
    send(AppEventHostLost);
    CHECK(app.screen == AppScreenWaiting);
    CHECK_EQ_INT(app.recent_count, 1); /* kept for the next host */
}

/* ---- Settings, About, transport ---- */

static AppEffect send_ble(AppBleStatus status) {
    AppEvent e = {.type = AppEventBleStatus, .ble = status};
    return app_reduce(&app, &e);
}

static void test_settings_rows_change_in_place(void) {
    setup();
    press(AppKeyRight);
    CHECK(app.screen == AppScreenSettings);
    CHECK_EQ_INT(app.sel, AppSettingConnection);
    CHECK(app_settings_connection(&app.settings) == AppConnectionAuto);
    press(AppKeyRight);
    CHECK(app_settings_connection(&app.settings) == AppConnectionUsb);
    CHECK(app.connection == AppConnectionAuto); /* not yet in use */
    press(AppKeyRight);
    CHECK(app_settings_connection(&app.settings) == AppConnectionBluetooth);
    press(AppKeyRight); /* wraps */
    CHECK(app_settings_connection(&app.settings) == AppConnectionAuto);
    press(AppKeyLeft);
    CHECK(app_settings_connection(&app.settings) == AppConnectionBluetooth);
    press(AppKeyLeft);
    CHECK(app_settings_connection(&app.settings) == AppConnectionUsb);
    press(AppKeyLeft);
    CHECK(app_settings_connection(&app.settings) == AppConnectionAuto);
    press(AppKeyDown);
    CHECK_EQ_INT(app.sel, AppSettingAutosave);
    press(AppKeyRight);
    CHECK(app_settings_autosave(&app.settings));
    press(AppKeyDown);
    CHECK_EQ_INT(app.sel, AppSettingAfterScan);
    press(AppKeyRight);
    CHECK(app_settings_after_scan(&app.settings) == AppAfterScanWeigh);
    press(AppKeyLeft);
    CHECK(app_settings_after_scan(&app.settings) == AppAfterScanShow);
    press(AppKeyLeft);
    CHECK(app_settings_after_scan(&app.settings) == AppAfterScanWeigh);
    press(AppKeyDown);
    CHECK_EQ_INT(app.sel, AppSettingWeighAs);
    press(AppKeyRight);
    CHECK(app_settings_weigh_as(&app.settings) == AppWeighAsNet);
    press(AppKeyRight);
    CHECK(app_settings_weigh_as(&app.settings) == AppWeighAsGross);
    press(AppKeyLeft);
    CHECK(app_settings_weigh_as(&app.settings) == AppWeighAsNet);
    CHECK(app.mode == WeightModeGross); /* the next Weight opens net */
    press(AppKeyDown);
    CHECK_EQ_INT(app.sel, AppSettingFeedback);
    press(AppKeyRight);
    CHECK(app_settings_feedback(&app.settings) == AppFeedbackLed);
    press(AppKeyRight);
    CHECK(app_settings_feedback(&app.settings) == AppFeedbackOff);
    press(AppKeyRight);
    CHECK(app_settings_feedback(&app.settings) == AppFeedbackVibroLed);
    press(AppKeyLeft);
    CHECK(app_settings_feedback(&app.settings) == AppFeedbackOff);
    press(AppKeyDown);
    press(AppKeyLeft);
    CHECK(app_settings_sound(&app.settings));
    press(AppKeyDown);
    CHECK_EQ_INT(app.sel, AppSettingVersion);
    CHECK_EQ_INT(app.top, 3);
    AppSettings before = app.settings;
    press(AppKeyRight);
    CHECK_MEM_EQ(&app.settings, &before, sizeof(before));
    press(AppKeyDown); /* wraps */
    CHECK_EQ_INT(app.sel, AppSettingConnection);
    CHECK_EQ_INT(app.top, 0);
    press(AppKeyOk); /* OK only on Version */
    CHECK(app.screen == AppScreenSettings);
    CHECK(press(AppKeyBack) == AppEffectSaveSettings);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.connection == AppConnectionAuto);
}

static void test_settings_back_without_changes_saves_nothing(void) {
    setup();
    press(AppKeyRight);
    press(AppKeyDown);
    press(AppKeyRight);
    press(AppKeyLeft); /* back to the stored value */
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenReady);
    /* reopening starts at the top */
    press(AppKeyRight);
    CHECK_EQ_INT(app.sel, 0);
}

static void test_settings_version_opens_about(void) {
    setup();
    press(AppKeyRight);
    press(AppKeyUp);
    CHECK_EQ_INT(app.sel, AppSettingVersion);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenAbout);
    CHECK(press(AppKeyOk) == AppEffectNone);
    CHECK(app.screen == AppScreenSettings);
    CHECK_EQ_INT(app.sel, AppSettingVersion);
    press(AppKeyOk);
    CHECK(press(AppKeyBack) == AppEffectNone);
    CHECK(app.screen == AppScreenSettings);
}

static void test_settings_returns_where_the_host_says(void) {
    single_slot_intent_store_init(&store);
    app_state_init(&app, &store.base, 1, 1);
    app_settings_set_first_run_done(&app.settings); /* not the first connection */
    press(AppKeyRight);
    CHECK(send(AppEventHostConnected) == AppEffectNone);
    CHECK(app.screen == AppScreenSettings);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenReady);
    press(AppKeyRight);
    send(AppEventHostLost);
    press(AppKeyBack);
    CHECK(app.screen == AppScreenWaiting);
}

static void test_changing_the_connection_switches_the_transport(void) {
    setup(); /* Auto */
    send_ble(AppBleAdvertising);
    press(AppKeyRight);
    press(AppKeyRight); /* USB */
    CHECK(press(AppKeyBack) == AppEffectSwitchTransport);
    CHECK(app.connection == AppConnectionUsb);
    CHECK(app.screen == AppScreenReady);
    CHECK(app.ble == AppBleOff); /* Bluetooth stops */
    press(AppKeyRight);
    press(AppKeyRight); /* Bluetooth */
    CHECK(press(AppKeyBack) == AppEffectSwitchTransport);
    CHECK(app.connection == AppConnectionBluetooth);
    send_ble(AppBleConnected);
    press(AppKeyRight);
    press(AppKeyRight); /* Auto */
    CHECK(press(AppKeyBack) == AppEffectSwitchTransport);
    CHECK(app.connection == AppConnectionAuto);
    CHECK(app.ble == AppBleConnected); /* Bluetooth keeps running */
}

static void test_leaving_settings_retries_a_failed_transport(void) {
    setup(); /* Auto, the host on USB */
    CHECK(send_via(AppEventTransportFailed, TransportKindBluetooth) == AppEffectNone);
    CHECK(app.transport_failed[TransportKindBluetooth]);
    CHECK(!app.transport_failed[TransportKindUsb]);
    CHECK(app.screen == AppScreenReady);
    press(AppKeyRight);
    CHECK(press(AppKeyBack) == AppEffectSwitchTransport);
    CHECK(app.connection == AppConnectionAuto);
    CHECK(!app.transport_failed[TransportKindBluetooth]);
    CHECK(app.connected); /* only what failed is opened again */
    /* it opened this time: leaving again changes nothing */
    press(AppKeyRight);
    CHECK(press(AppKeyBack) == AppEffectNone);

    /* a failed transport the new connection does not use is forgotten */
    send_via(AppEventTransportFailed, TransportKindBluetooth);
    press(AppKeyRight);
    press(AppKeyRight); /* USB */
    CHECK(press(AppKeyBack) == AppEffectSwitchTransport);
    CHECK(!app.transport_failed[TransportKindBluetooth]);
    press(AppKeyRight);
    CHECK(press(AppKeyBack) == AppEffectNone);

    /* USB alone failed: retried */
    send_via(AppEventTransportFailed, TransportKindUsb);
    press(AppKeyRight);
    CHECK(press(AppKeyBack) == AppEffectSwitchTransport);
    CHECK(!app.transport_failed[TransportKindUsb]);
}

static void test_transport_failure_and_ble_status_on_every_screen(void) {
    setup();
    scan_mfc();
    CHECK(send(AppEventTransportFailed) == AppEffectNone);
    CHECK(app.transport_failed[TransportKindUsb]);
    CHECK(app.screen == AppScreenLookingUp);

    setup();
    app.connection = AppConnectionBluetooth;
    CHECK(send_ble(AppBleAdvertising) == AppEffectNone);
    CHECK(app.ble == AppBleAdvertising);
    CHECK(app.screen == AppScreenReady);
    scan_mfc();
    CHECK(send_ble(AppBleConnected) == AppEffectNone);
    CHECK(app.ble == AppBleConnected);
    CHECK(app.screen == AppScreenLookingUp);
    /* the status alone does not lose the host: host_link does */
    CHECK(send_ble(AppBleOff) == AppEffectNone);
    CHECK(app.connected);
}

static void test_cue_is_reset_by_every_event(void) {
    setup();
    CHECK(app.cue == AppCueConnected);
    tick(10);
    CHECK(app.cue == AppCueNone);
    send(AppEventHostConnected); /* again, e.g. after a reconnect */
    CHECK(app.cue == AppCueConnected);
    CHECK(app.screen == AppScreenReady);
}

int main(void) {
    RUN_TEST(test_init_waits_for_a_host);
    RUN_TEST(test_waiting_and_ready_keys);
    RUN_TEST(test_get_app_keys);
    RUN_TEST(test_a_connecting_app_moves_get_app_on_to_ready);
    RUN_TEST(test_the_first_start_opens_get_app_once);
    RUN_TEST(test_start_after_an_app_connected_stays_on_waiting);
    RUN_TEST(test_a_later_connection_saves_nothing);
    RUN_TEST(test_a_tag_on_get_app_shows_its_facts);
    RUN_TEST(test_scan_gets_a_new_id_and_a_copy_of_the_tag);
    RUN_TEST(test_scan_carries_the_autosave_setting);
    RUN_TEST(test_read_failed_returns_to_ready_by_itself);
    RUN_TEST(test_unsupported_tag_is_its_own_quiet_outcome);
    RUN_TEST(test_forget_the_tag_when_the_result_is_left_unanswered);
    RUN_TEST(test_other_host_version_is_remembered_per_transport_until_a_host_connects);
    RUN_TEST(test_a_connected_host_names_its_transport);
    RUN_TEST(test_unknown_right_links_and_known_right_marks_new);
    RUN_TEST(test_read_failed_and_back_while_reading);
    RUN_TEST(test_looking_up_back_and_stale_lookups);
    RUN_TEST(test_no_host_scan_shows_local_facts);
    RUN_TEST(test_a_scan_that_cannot_be_sent_shows_local_facts);
    RUN_TEST(test_lookup_error_is_not_retryable);
    RUN_TEST(test_reader_runs_where_a_tag_is_welcome);
    RUN_TEST(test_a_tag_on_a_result_rescans_but_not_on_the_menu);
    RUN_TEST(test_found_weigh_saved_then_ready);
    RUN_TEST(test_saved_ok_goes_to_ready_at_once);
    RUN_TEST(test_result_known_keys);
    RUN_TEST(test_archived_spool_is_known);
    RUN_TEST(test_weight_digits_and_cursor);
    RUN_TEST(test_mode_chip_converts_the_number);
    RUN_TEST(test_mode_chip_floors_net_at_zero);
    RUN_TEST(test_recent_opens_from_ready_and_starts_empty);
    RUN_TEST(test_a_saved_weight_goes_to_the_top_of_recent);
    RUN_TEST(test_recent_keeps_the_last_six);
    RUN_TEST(test_recent_entry_opens_its_result);
    RUN_TEST(test_recent_follows_marks_and_deletes);
    RUN_TEST(test_losing_the_host_leaves_recent);
    RUN_TEST(test_weight_opens_in_the_weigh_as_mode);
    RUN_TEST(test_after_scan_weigh_opens_weight_for_a_known_spool);
    RUN_TEST(test_save_below_core_is_refused);
    RUN_TEST(test_net_or_unknown_core_never_refuses);
    RUN_TEST(test_weight_prefill_falls_back_to_the_defaults);
    RUN_TEST(test_menu_items_per_kind);
    RUN_TEST(test_menu_selection_wraps_and_scrolls);
    RUN_TEST(test_menu_weigh_and_details);
    RUN_TEST(test_delete_only_from_the_confirmation);
    RUN_TEST(test_deleted_toast_back_clears_the_bar);
    RUN_TEST(test_mark_as_new_undo_sends_nothing);
    RUN_TEST(test_mark_as_new_commits_when_the_bar_runs_out);
    RUN_TEST(test_mark_as_new_ok_commits_then_weighs);
    RUN_TEST(test_mark_as_new_back_commits);
    RUN_TEST(test_mark_as_empty_commits_on_a_new_tag);
    RUN_TEST(test_mark_as_empty_ok_and_undo);
    RUN_TEST(test_unknown_add_shows_added);
    RUN_TEST(test_added_toast_next_and_back);
    RUN_TEST(test_autosave_lookup_created_goes_to_added);
    RUN_TEST(test_unknown_back_creates_nothing);
    RUN_TEST(test_add_and_weigh_sends_weigh_on_the_unknown_scan);
    RUN_TEST(test_link_on_host_then_fresh_lookup);
    RUN_TEST(test_link_on_host_cancel_and_late_lookup);
    RUN_TEST(test_relink_and_create_on_host);
    RUN_TEST(test_stale_lookups_on_other_screens_are_ignored);
    RUN_TEST(test_results_not_in_flight_are_ignored);
    RUN_TEST(test_failed_intent_retries_with_the_same_id);
    RUN_TEST(test_error_back_returns_to_the_result);
    RUN_TEST(test_timeout_then_retry);
    RUN_TEST(test_timeout_survives_the_tick_wrap);
    RUN_TEST(test_an_intent_that_cannot_be_sent_is_an_error);
    RUN_TEST(test_send_failed_elsewhere_is_ignored);
    RUN_TEST(test_host_lost_mid_intent_retries_after_reconnect);
    RUN_TEST(test_failed_commit_of_mark_as_new_retries_then_weighs);
    RUN_TEST(test_host_lost_on_screens_about_the_tag);
    RUN_TEST(test_host_lost_on_other_screens);
    RUN_TEST(test_settings_rows_change_in_place);
    RUN_TEST(test_settings_back_without_changes_saves_nothing);
    RUN_TEST(test_settings_version_opens_about);
    RUN_TEST(test_settings_returns_where_the_host_says);
    RUN_TEST(test_changing_the_connection_switches_the_transport);
    RUN_TEST(test_leaving_settings_retries_a_failed_transport);
    RUN_TEST(test_transport_failure_and_ble_status_on_every_screen);
    RUN_TEST(test_cue_is_reset_by_every_event);
    return MINITEST_RESULT();
}
