#include "kyberwrite_scene.h"
#include <notification/notification_messages.h>
#include <notification/notification.h>

// ── Crystal RGB colors ────────────────────────────────────────────────────────
typedef struct { uint8_t r, g, b; } KyberColor;

static const KyberColor KYBER_COLORS[] = {
    { 200, 200, 200 }, // White  (Palpatine/Ahsoka)
    { 255,   0,   0 }, // Red    (Vader/Yoda)
    { 255,  60,   0 }, // Orange
    { 220, 120,   0 }, // Yellow (Palpatine/Guard)
    {   0, 255,   0 }, // Green  (Palpatine/Qui-Gon)
    {   0, 220, 220 }, // Cyan
    {   0,   0, 255 }, // Blue   (Palpatine/Obi-Wan)
    { 255,   0, 255 }, // Purple (Palpatine/Mace 1)
    { 200, 200, 200 }, // White  (Palpatine/Chirrut)
    { 255,   0,   0 }, // Red    (Palpatine/Yoda)
    { 255,   0,   0 }, // Red    (Dooku/Yoda)
    { 220, 220,   0 }, // Yellow (Palpatine/Maz)
    {   0, 255,   0 }, // Green  (Palpatine/Yoda)
    { 255,   0,   0 }, // Red    (Maul/Yoda)
    {   0,   0, 255 }, // Blue   (Palpatine/Old Luke)
    { 255,   0, 255 }, // Purple (Palpatine/Mace 2)
    { 255,   0,   0 }, // Red    (Vader 8-Ball/Yoda)
    {   0, 255,   0 }, // Green  (Palpatine/Yoda 8-B)
    {  255,  125,  125 }, // Black  (Snoke/Yoda) — dim white
};

// ── LED helpers ───────────────────────────────────────────────────────────────
// Use module-level static messages so pointers are always valid.
// notification_message() on Flipper is synchronous for LED messages,
// but keeping them static is safest.

static NotificationMessage s_msg_r = { .type = NotificationMessageTypeLedRed };
static NotificationMessage s_msg_g = { .type = NotificationMessageTypeLedGreen };
static NotificationMessage s_msg_b = { .type = NotificationMessageTypeLedBlue };
// message_do_not_reset prevents the notification service from zeroing all
// LED channels after the sequence finishes.
static const NotificationMessage* s_rgb_seq[] = {
    &s_msg_r, &s_msg_g, &s_msg_b, &message_do_not_reset, NULL
};

static void set_rgb_led(KyberApp* app, uint8_t r, uint8_t g, uint8_t b) {
    s_msg_r.data.led.value = r;
    s_msg_g.data.led.value = g;
    s_msg_b.data.led.value = b;
    notification_message(app->notifications, (const NotificationSequence*)s_rgb_seq);
}

static void set_crystal_led(KyberApp* app, int crystal_index) {
    KyberColor c = KYBER_COLORS[crystal_index];
    set_rgb_led(app, c.r, c.g, c.b);
}

static void clear_led(KyberApp* app) {
    set_rgb_led(app, 0, 0, 0);
}

// ── Match addr6 against crystal table ────────────────────────────────────────
static int kyber_match_addr6(uint32_t addr6) {
    for(int i = 0; i < (int)KYBER_CRYSTAL_COUNT; i++) {
        if(KYBER_CRYSTALS[i].addr6 == addr6) return i;
    }
    return -1;
}

// ── Worker read callback ──────────────────────────────────────────────────────
static void read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    furi_assert(context);
    KyberApp* app = context;
    if(result == LFRFIDWorkerReadDone) {
        scene_manager_set_scene_state(
            app->scene_manager, KyberSceneRead, (uint32_t)protocol);
        view_dispatcher_send_custom_event(app->view_dispatcher, KyberEventReadDone);
    }
}

// ── Widgets ───────────────────────────────────────────────────────────────────
static void show_waiting(KyberApp* app) {
    widget_reset(app->read_widget);
    widget_add_string_element(
        app->read_widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Read Crystal");
    widget_add_string_element(
        app->read_widget, 64, 32, AlignCenter, AlignCenter, FontSecondary,
        "Hold crystal near Flipper");
    widget_add_string_element(
        app->read_widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, "Back to cancel");
}

static void show_result(KyberApp* app, ProtocolId protocol) {
    widget_reset(app->read_widget);

    if(protocol != LFRFIDProtocolEM4100 &&
       protocol != LFRFIDProtocolEM4100_32 &&
       protocol != LFRFIDProtocolEM4100_16) {
        widget_add_string_element(
            app->read_widget, 64, 10, AlignCenter, AlignCenter, FontPrimary, "Not a Kyber Crystal");
        widget_add_string_element(
            app->read_widget, 64, 32, AlignCenter, AlignCenter, FontSecondary,
            "EM4100 tag not detected");
        widget_add_string_element(
            app->read_widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, "Back to retry");
        clear_led(app);
        return;
    }

    LFRFIDWriteRequest request = {.write_type = LFRFIDWriteTypeEM4305};
    bool ok = protocol_dict_get_write_data(app->protocol_dict, (size_t)protocol, &request);

    widget_add_string_element(
        app->read_widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Crystal Identified");

    if(!ok) {
        widget_add_string_element(
            app->read_widget, 64, 32, AlignCenter, AlignCenter, FontSecondary,
            "Could not decode tag");
        clear_led(app);
        return;
    }

    uint32_t addr6 = request.em4305.word[6];
    int match      = kyber_match_addr6(addr6);

    if(match >= 0) {
        widget_add_string_element(
            app->read_widget, 64, 24, AlignCenter, AlignCenter, FontSecondary,
            KYBER_CRYSTALS[match].name);
        static char hex_buf[12];
        snprintf(hex_buf, sizeof(hex_buf), "%08lX", (unsigned long)addr6);
        widget_add_string_element(
            app->read_widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, hex_buf);
        set_crystal_led(app, match);
    } else {
        widget_add_string_element(
            app->read_widget, 64, 24, AlignCenter, AlignCenter, FontSecondary,
            "Unknown crystal code:");
        static char hex_buf2[12];
        snprintf(hex_buf2, sizeof(hex_buf2), "%08lX", (unsigned long)addr6);
        widget_add_string_element(
            app->read_widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, hex_buf2);
        clear_led(app);
    }

    widget_add_string_element(
        app->read_widget, 64, 56, AlignCenter, AlignCenter, FontSecondary, "Back to scan again");
}

// ── Scene ─────────────────────────────────────────────────────────────────────
void kyberwrite_scene_read_on_enter(void* context) {
    furi_assert(context);
    KyberApp* app = context;

    show_waiting(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, KyberViewRead);
    set_rgb_led(app, 0, 0, 40); // dim blue while waiting
    scene_manager_set_scene_state(app->scene_manager, KyberSceneRead, PROTOCOL_NO);
    lfrfid_worker_read_start(app->rfid_worker, LFRFIDWorkerReadTypeASKOnly, read_callback, app);
}

bool kyberwrite_scene_read_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KyberApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == KyberEventReadDone) {
            lfrfid_worker_stop(app->rfid_worker);
            ProtocolId protocol = (ProtocolId)scene_manager_get_scene_state(
                app->scene_manager, KyberSceneRead);
            show_result(app, protocol);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        lfrfid_worker_stop(app->rfid_worker);
        ProtocolId protocol = (ProtocolId)scene_manager_get_scene_state(
            app->scene_manager, KyberSceneRead);

        if(protocol == PROTOCOL_NO) {
            clear_led(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, KyberSceneMenu);
        } else {
            show_waiting(app);
            set_rgb_led(app, 0, 0, 40);
            scene_manager_set_scene_state(app->scene_manager, KyberSceneRead, PROTOCOL_NO);
            lfrfid_worker_read_start(
                app->rfid_worker, LFRFIDWorkerReadTypeASKOnly, read_callback, app);
        }
        consumed = true;
    }
    return consumed;
}

void kyberwrite_scene_read_on_exit(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    lfrfid_worker_stop(app->rfid_worker);
    clear_led(app);
    widget_reset(app->read_widget);
}
