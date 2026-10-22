#include "../sigroam.h"
#include "../src/sr_dialect.h"
#include "../src/sr_last_upload.h"
#include "../src/sr_marauder_join.h"
#include "../src/sr_marauder_upload.h"
#include "../src/sr_mau_logs.h"
#include "../src/sr_uart_cap.h"
#include "../src/sr_upload_prompt.h"
#include "../src/sr_view_fmt.h"

#include <input/input.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>

_Static_assert(
    (unsigned)SR_UPLOAD_INFO_PERIOD_TICKS == (unsigned)SR_QUAL_REFRESH_PERIOD_TICKS,
    "Upload info period must match Dash qual refresh");
_Static_assert(
    (unsigned)SR_UPLOAD_STATUS_PERIOD_TICKS*(unsigned)SR_TICK_PERIOD_MS == 2000u,
    "uploadstatus stays on the 2 s cadence");
_Static_assert(sizeof("disc ") - 1u + 10u <= 20u, "disc line fits 20 columns");
_Static_assert((unsigned)SrSessionIdle == 0u, "mau allow_go idle is 0");
_Static_assert((unsigned)SrSessionRunning == 1u, "mau allow_go running is 1");
_Static_assert((unsigned)SrSessionStopped == 2u, "mau allow_go stopped is 2");

enum {
    SigRoamUploadEventGo = 1,
    SigRoamUploadEventWifi = 2,
    SigRoamUploadEventHelp = 3,
    SigRoamUploadEventLogs = 4,
};

enum {
    UploadPageResult = 0,
    UploadPageHelp = 1,
};

_Static_assert(sizeof("Key imported (boot)") - 1u <= SR_MAU_COLS, "key imported");
_Static_assert(sizeof("Saw 1 of 2 key files") - 1u <= SR_MAU_COLS, "key partial");
/* Help: existing 10 lines + key + Not sent yet + 6 names + +N more + URL. */
_Static_assert(
    20u * ((size_t)SR_MAU_COLS + 1u) <= (size_t)SR_UPLOAD_TEXT_MAX,
    "help page fits the upload text buffer");
_Static_assert(
    sizeof("WiGLE upload") + 6u * ((size_t)SR_MAU_COLS + 1u) <= (size_t)SR_UPLOAD_TEXT_MAX,
    "result page fits the upload text buffer");
_Static_assert(sizeof("Upload on PC") - 1u <= SR_MAU_COLS, "upload on pc");
_Static_assert(sizeof("Not sent yet:") - 1u <= SR_MAU_COLS, "not sent yet");
_Static_assert(sizeof("wigle.net/uploads") - 1u <= SR_MAU_COLS, "wigle url");

static uint8_t s_page;
static unsigned s_block;
static uint32_t s_logs_rev;

static uint32_t s_mau_rev_shown;
static bool s_cap_sent;
static bool s_cap_ending;
static uint32_t s_cap_end_ms;
static SrLastUpload s_last;
static unsigned s_last_st;
static bool s_lite_flew;
static bool s_lite_stored;
static bool s_lite_refresh;

#define SR_LAST_UPLOAD_PATH APP_DATA_PATH("last_upload.txt")

static bool upload_lite(const SigRoamApp* app) {
    return sr_dialect_lite_status(&app->model.firmware);
}

static void upload_load_last(void) {
    Storage* storage;
    File* file;
    char buf[96];
    uint64_t size;
    size_t nread;
    SrLastUpload parsed;

    s_last_st = (unsigned)SrLastNone;
    memset(&s_last, 0, sizeof(s_last));
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    if(!storage_file_open(file, SR_LAST_UPLOAD_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }
    size = storage_file_size(file);
    s_last_st = (unsigned)SrLastBad;
    if(size > 0u && size < sizeof(buf)) {
        nread = storage_file_read(file, buf, (size_t)size);
        if(nread == (size_t)size && sr_last_upload_parse(buf, nread, &parsed) == SrLastOk) {
            s_last = parsed;
            s_last_st = (unsigned)SrLastOk;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static bool upload_store_last(void) {
    Storage* storage;
    File* file;
    char buf[96];
    size_t n;
    SrMauUpTally tally;
    SrLastUpload u;
    DateTime dt;
    bool opened;

    if(!sr_mau_up_tally(&tally) || !tally.ready) {
        return false;
    }
    furi_hal_rtc_get_datetime(&dt);
    memset(&u, 0, sizeof(u));
    u.month = dt.month;
    u.day = dt.day;
    u.hour = dt.hour;
    u.minute = dt.minute;
    u.ok_n = tally.ok_n;
    u.fail_n = tally.fail_n;
    u.wigle200 = tally.wigle200;
    n = sr_last_upload_format(&u, buf, sizeof(buf));
    s_last_st = (unsigned)SrLastBad;
    if(n == 0u) {
        return true;
    }
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    opened = storage_file_open(file, SR_LAST_UPLOAD_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(opened && storage_file_write(file, buf, n) == n) {
        s_last = u;
        s_last_st = (unsigned)SrLastOk;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

static void upload_cap_begin(void) {
    sr_uart_cap_arm(false);
    sr_uart_cap_mark("upload");
    s_cap_sent = true;
    s_cap_ending = false;
    s_cap_end_ms = 0u;
}

static void sigroam_scene_upload_button(GuiButtonType result, InputType type, void* context);

static bool sigroam_scene_upload_hold(const SigRoamApp* app) {
    return sr_upload_prompt_hold(
        app->model.firmware.diag_seen, app->model.firmware.diag_state, app->model.up.reason);
}

/* Status wins the shared tick. Info only when this poll says so and the slot accepts it. */
static void sigroam_scene_upload_poll(SigRoamApp* app) {
    bool sigroam = sr_dialect_is_sigroam(&app->model.firmware);
    bool generic = sr_dialect_is_generic_marauder(&app->model.firmware);
    int poll;

    if(app->worker == NULL) {
        return;
    }
    /* Generic Marauder has no uploadstatus, and info leaves SHOW_INFO set. */
    if(generic) {
        return;
    }
    poll = sr_upload_poll_cmd(
        app->tick_n, sigroam, generic, app->upload_ident_info_sent, app->upload_info_armed);
    if(poll == SR_UPLOAD_POLL_STATUS) {
        (void)sr_worker_send_cmd(app->worker, sr_upload_enter_cmd(false));
        return;
    }
    if(poll != SR_UPLOAD_POLL_INFO) {
        return;
    }
    if(!sr_worker_send_cmd(app->worker, "info\n")) {
        return;
    }
    app->upload_info_armed = false;
    if(!sigroam) {
        app->upload_ident_info_sent = true;
    }
}

static bool sigroam_scene_upload_sd_dead(const SigRoamApp* app) {
    return sr_scan_ctl_sd_dead(
        app->model.qual_rev, app->model.qual.sd, app->model.qual_tick_ms, furi_get_tick());
}

static void sigroam_scene_upload_append_cfg(SigRoamApp* app) {
    char cfg[24];
    size_t n;
    size_t c;

    if(app->model.cfg_rev == 0u) {
        return;
    }
    c = sr_fmt_cfg_line(
        app->model.cfg.key,
        app->model.cfg.home,
        app->model.cfg.ssid,
        (size_t)SR_VIEW_COLS,
        cfg,
        sizeof(cfg));
    n = 0u;
    while(n < sizeof(app->upload_text) && app->upload_text[n] != '\0') {
        n++;
    }
    if(n + 1u + c + 1u > sizeof(app->upload_text)) {
        return;
    }
    app->upload_text[n++] = '\n';
    memcpy(app->upload_text + n, cfg, c);
    app->upload_text[n + c] = '\0';
}

static void sigroam_scene_upload_add(char* dst, size_t cap, const char* src) {
    size_t n = 0u;
    size_t i;

    if(dst == NULL || src == NULL || src[0] == '\0') {
        return;
    }
    while(n < cap && dst[n] != '\0') {
        n++;
    }
    if(n > 0u && n + 1u < cap) {
        dst[n++] = '\n';
    }
    for(i = 0u; src[i] != '\0' && n + 1u < cap; i++) {
        dst[n++] = src[i];
    }
    if(n < cap) {
        dst[n] = '\0';
    }
}

static void sigroam_scene_upload_fill_generic(SigRoamApp* app) {
    char line1[SR_MAU_COLS + 1u];
    char line2[SR_MAU_COLS + 1u];
    char line3[SR_MAU_COLS + 1u];
    char sent[SR_MAU_COLS + 1u];
    char hint[SR_MAU_COLS + 1u];
    uint32_t unsent;

    sr_mau_up_lines((unsigned)app->model.session, line1, line2);
    sr_mau_up_context(line3);
    sent[0] = '\0';
    (void)sr_mau_logs_sent_line(sent);
    sr_mau_up_hint(hint);
    snprintf(
        app->upload_text,
        sizeof(app->upload_text),
        "WiGLE upload\n%s\n%s%s%s%s%s",
        line1,
        line2,
        line3[0] != '\0' ? "\n" : "",
        line3,
        sent[0] != '\0' ? "\n" : "",
        sent);
    unsent = sr_mau_logs_unsent();
    if(unsent != SR_MAU_LOGS_UNKNOWN && unsent > 0u) {
        sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), "Upload on PC");
    }
    sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), hint);
}

static void sigroam_scene_upload_fill_help(SigRoamApp* app) {
    char line1[SR_MAU_COLS + 1u];
    char line2[SR_MAU_COLS + 1u];
    char card[SR_MAU_COLS + 1u];
    const char* head1 = SR_MAU_LOGS_HELP_1;
    const char* head2 = SR_MAU_LOGS_HELP_2;
    const char* key = "";
    unsigned key_state = sr_mau_up_key_state();

    if(s_block != SrMauLogsBlockNone) {
        sr_mau_logs_block_lines(s_block, line1, line2);
    } else {
        sr_mau_logs_lines(line1, line2);
    }
    if(sr_mau_logs_card_line(card) && sr_mau_up_empty_list()) {
        head1 = card;
        head2 = SR_MAU_LOGS_SEEN_2;
    }
    if(key_state == SrMauKeyImported) {
        key = "\nKey imported (boot)";
    } else if(key_state == SrMauKeyPartial) {
        key = "\nSaw 1 of 2 key files";
    }
    snprintf(
        app->upload_text,
        sizeof(app->upload_text),
        "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s%s",
        head1,
        head2,
        SR_MAU_LOGS_HELP_3,
        SR_MAU_LOGS_HELP_4,
        line1,
        line2,
        SR_MAU_KEY_HELP_1,
        SR_MAU_KEY_HELP_2,
        SR_MAU_KEY_HELP_3,
        SR_MAU_KEY_HELP_4,
        key);
    {
        uint32_t n = sr_mau_logs_unsent();
        unsigned i;
        char row[SR_MAU_COLS + 1u];
        char more[SR_MAU_COLS + 1u];

        if(n != SR_MAU_LOGS_UNKNOWN && n > 0u) {
            sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), "Not sent yet:");
            for(i = 0u; i < 6u && i < (unsigned)n; i++) {
                if(sr_mau_logs_unsent_entry(i, row)) {
                    sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), row);
                }
            }
            if(n > 6u) {
                snprintf(more, sizeof(more), "+%lu more", (unsigned long)(n - 6u));
                sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), more);
            }
            sigroam_scene_upload_add(
                app->upload_text, sizeof(app->upload_text), "wigle.net/uploads");
        }
    }
}

static void sigroam_scene_upload_fill_lite(SigRoamApp* app) {
    char last[40];
    char pend[32];
    char line1[SR_MAU_COLS + 1u];
    char line2[SR_MAU_COLS + 1u];

    (void)sr_last_upload_line(s_last_st, &s_last, last, sizeof(last));
    (void)sr_last_upload_pending(app->model.up_rev != 0u, app->model.up.q, pend, sizeof(pend));
    sr_mau_up_lines((unsigned)app->model.session, line1, line2);
    snprintf(
        app->upload_text, sizeof(app->upload_text), "%s\n%s\n%s\n%s", last, pend, line1, line2);
    if(app->model.cfg_rev != 0u && app->model.cfg.key == 0u) {
        sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), "No WiGLE key");
        sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), "See Help");
    }
    if(app->model.cfg_rev != 0u && app->model.cfg.home == 0u) {
        sigroam_scene_upload_add(app->upload_text, sizeof(app->upload_text), "No saved Wi-Fi");
    }
}

static void sigroam_scene_upload_fill(SigRoamApp* app) {
    const SrUpInfo* u = &app->model.up;
    SrUploadPrompt prompt;
    uint8_t st = app->model.firmware.diag_state;
    bool seen = app->model.up_rev != 0u;

    if(upload_lite(app) && s_page == UploadPageHelp) {
        sigroam_scene_upload_fill_help(app);
        return;
    }
    if(upload_lite(app)) {
        sigroam_scene_upload_fill_lite(app);
        return;
    }
    if(sr_dialect_is_generic_marauder(&app->model.firmware)) {
        if(s_page == UploadPageHelp) {
            sigroam_scene_upload_fill_help(app);
        } else {
            sigroam_scene_upload_fill_generic(app);
        }
        return;
    }

    if(sr_upload_prompt(
           app->model.firmware.diag_seen,
           st,
           app->model.up_rev,
           u->last_trans,
           u->reason,
           sigroam_scene_upload_sd_dead(app),
           &prompt)) {
        snprintf(
            app->upload_text,
            sizeof(app->upload_text),
            "WiGLE upload\n"
            "q=%lu\n"
            "disc %lu\n"
            "%s\n"
            "%s",
            seen ? (unsigned long)u->q : 0ul,
            seen ? (unsigned long)u->disc_gps : 0ul,
            prompt.line1,
            prompt.line2);
        sigroam_scene_upload_append_cfg(app);
        return;
    }

    snprintf(
        app->upload_text,
        sizeof(app->upload_text),
        "WiGLE upload\n"
        "q=%lu\n"
        "disc %lu\n"
        "%s",
        seen ? (unsigned long)u->q : 0ul,
        seen ? (unsigned long)u->disc_gps : 0ul,
        (app->model.firmware.diag_seen && st == 1u) ? "OK: stop+up" : "OK: upload");
    sigroam_scene_upload_append_cfg(app);
}

static uint32_t sigroam_scene_upload_hash(const char* s) {
    uint32_t h = 2166136261u;

    while(*s != '\0') {
        h = (h ^ (uint8_t)*s++) * 16777619u;
    }
    return h;
}

/* force: first draw on enter. Otherwise rebuild only when the text changed. */
static void sigroam_scene_upload_draw(SigRoamApp* app, bool force) {
    uint32_t h;

    sigroam_scene_upload_fill(app);
    h = sigroam_scene_upload_hash(app->upload_text);
    if(!force && h == app->upload_text_hash) {
        return;
    }
    app->upload_text_hash = h;
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget, 0, 0, SR_CANVAS_W, (uint8_t)(SR_CANVAS_H - 14), app->upload_text);
    if(sr_dialect_is_generic_marauder(&app->model.firmware) && !upload_lite(app) &&
       s_page == UploadPageHelp) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Logs", sigroam_scene_upload_button, app);
        return;
    }
    if(upload_lite(app) && s_page == UploadPageHelp) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Logs", sigroam_scene_upload_button, app);
        return;
    }
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Upload", sigroam_scene_upload_button, app);
    if(upload_lite(app)) {
        if(app->model.cfg_rev != 0u && app->model.cfg.home == 0u &&
           sr_mau_allow_go((unsigned)app->model.session, sr_mau_up_in_flight())) {
            widget_add_button_element(
                app->widget, GuiButtonTypeLeft, "WiFi", sigroam_scene_upload_button, app);
        }
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Help", sigroam_scene_upload_button, app);
        return;
    }
    if(sr_dialect_is_generic_marauder(&app->model.firmware) &&
       sr_mau_allow_go((unsigned)app->model.session, sr_mau_up_in_flight())) {
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "WiFi", sigroam_scene_upload_button, app);
    }
    if(sr_dialect_is_generic_marauder(&app->model.firmware)) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Help", sigroam_scene_upload_button, app);
    }
}

static void sigroam_scene_upload_button(GuiButtonType result, InputType type, void* context) {
    SigRoamApp* app = context;

    if(app == NULL || type != InputTypeShort) {
        return;
    }
    if(result == GuiButtonTypeCenter) {
        uint32_t ev =
            (sr_dialect_is_generic_marauder(&app->model.firmware) && s_page == UploadPageHelp) ?
                (uint32_t)SigRoamUploadEventLogs :
                (uint32_t)SigRoamUploadEventGo;
        view_dispatcher_send_custom_event(app->view_dispatcher, ev);
    } else if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SigRoamUploadEventWifi);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SigRoamUploadEventHelp);
    }
}

void sigroam_scene_upload_on_enter(void* context) {
    SigRoamApp* app = context;

    app->upload_up_rev_shown = app->model.up_rev;
    app->upload_fw_rev_shown = app->model.firmware_rev;
    app->upload_qual_rev_shown = app->model.qual_rev;
    app->upload_cfg_rev_shown = app->model.cfg_rev;
    app->upload_go_retry = false;
    app->upload_info_armed = true;
    app->upload_ident_info_sent = false;
    s_page = UploadPageResult;
    s_block = SrMauLogsBlockNone;
    sr_mau_logs_reset();
    s_logs_rev = 0u;
    if(sr_dialect_is_generic_marauder(&app->model.firmware) && !sr_mau_up_in_flight()) {
        sr_mau_up_reset();
    }
    s_lite_flew = sr_mau_up_in_flight();
    s_lite_stored = false;
    s_lite_refresh = false;
    if(upload_lite(app)) {
        upload_load_last();
        if(app->worker != NULL && !sr_mau_up_in_flight()) {
            (void)sr_worker_send_cmd(app->worker, "sigroam\n");
        }
    }
    if(app->worker != NULL && !sr_dialect_is_generic_marauder(&app->model.firmware)) {
        (void)sr_worker_send_cmd(app->worker, sr_upload_enter_cmd(false));
    }
    sigroam_scene_upload_draw(app, true);
    s_mau_rev_shown = sr_mau_up_rev();
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewWidget);
}

bool sigroam_scene_upload_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeBack && s_page == UploadPageHelp) {
        s_page = UploadPageResult;
        s_block = SrMauLogsBlockNone;
        sigroam_scene_upload_draw(app, true);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == SigRoamUploadEventHelp) {
        if(sr_dialect_is_generic_marauder(&app->model.firmware)) {
            s_page = UploadPageHelp;
            s_block = SrMauLogsBlockNone;
            sigroam_scene_upload_draw(app, true);
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == SigRoamUploadEventLogs) {
        bool generic = sr_dialect_is_generic_marauder(&app->model.firmware);
        bool upload_flight = sr_mau_up_in_flight();
        bool join_busy = sr_join_listening();
        unsigned session = (unsigned)app->model.session;

        if(!generic || s_page != UploadPageHelp || sr_mau_logs_armed()) {
            return true;
        }
        if(!sr_mau_logs_allow(session, upload_flight, join_busy)) {
            if(session == (unsigned)SrSessionRunning) {
                s_block = SrMauLogsBlockScan;
            } else if(upload_flight) {
                s_block = SrMauLogsBlockUpload;
            } else {
                s_block = SrMauLogsBlockJoin;
            }
            sigroam_scene_upload_draw(app, true);
            return true;
        }
        sr_mau_logs_arm();
        s_block = SrMauLogsBlockNone;
        if(app->worker == NULL || !sr_worker_send_cmd(app->worker, sr_mau_logs_cmd())) {
            sr_mau_logs_reset();
            s_block = SrMauLogsBlockBusy;
        }
        s_logs_rev = sr_mau_logs_rev();
        sigroam_scene_upload_draw(app, true);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == SigRoamUploadEventWifi) {
        if(s_page == UploadPageHelp) {
            return true;
        }
        if(sr_dialect_is_generic_marauder(&app->model.firmware) &&
           sr_mau_allow_go((unsigned)app->model.session, sr_mau_up_in_flight())) {
            scene_manager_next_scene(app->scene_manager, SigRoamSceneJoin);
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == SigRoamUploadEventGo) {
        bool generic = sr_dialect_is_generic_marauder(&app->model.firmware);

        if(generic && s_page == UploadPageHelp) {
            return true;
        }

        if(generic) {
            if(!sr_mau_allow_go((unsigned)app->model.session, sr_mau_up_in_flight())) {
                app->upload_go_retry = false;
                return true;
            }
        } else if(sigroam_scene_upload_sd_dead(app) || sigroam_scene_upload_hold(app)) {
            app->upload_go_retry = false;
            return true;
        }
        if(app->worker != NULL && sr_worker_send_cmd(app->worker, sr_upload_go_cmd(generic))) {
            if(generic) {
                upload_cap_begin();
                sr_mau_up_on_send();
            }
            app->upload_go_retry = false;
        } else {
            app->upload_go_retry = true;
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeTick) {
        bool generic = sr_dialect_is_generic_marauder(&app->model.firmware);
        bool lite_sent = false;

        if(upload_lite(app)) {
            if(sr_mau_up_in_flight()) {
                s_lite_flew = true;
                s_lite_stored = false;
                s_lite_refresh = false;
            } else if(s_lite_flew) {
                if(!s_lite_stored && upload_store_last()) {
                    s_lite_stored = true;
                    sigroam_scene_upload_draw(app, true);
                }
                if(!s_lite_refresh && app->worker != NULL &&
                   sr_worker_send_cmd(app->worker, "sigroam\n")) {
                    s_lite_refresh = true;
                    lite_sent = true;
                }
            }
        }

        if(sr_mau_logs_barrier_send(generic, lite_sent) && app->worker != NULL &&
           sr_worker_send_cmd(app->worker, sr_mau_logs_barrier_cmd())) {
            sr_mau_logs_barrier_sent();
        }

        if(generic && s_cap_sent && sr_uart_cap_armed() && !sr_mau_up_in_flight()) {
            uint32_t now = furi_get_tick();

            if(!s_cap_ending) {
                sr_uart_cap_mark("upload-end");
                s_cap_ending = true;
                s_cap_end_ms = now;
            } else if((uint32_t)(now - s_cap_end_ms) >= 3000u) {
                sr_uart_cap_disarm();
                s_cap_sent = false;
                s_cap_ending = false;
            }
        }
        if(!lite_sent && app->upload_go_retry && app->worker != NULL) {
            if(generic) {
                if(!sr_mau_allow_go((unsigned)app->model.session, sr_mau_up_in_flight())) {
                    app->upload_go_retry = false;
                } else if(sr_worker_send_cmd(app->worker, sr_upload_go_cmd(true))) {
                    upload_cap_begin();
                    sr_mau_up_on_send();
                    app->upload_go_retry = false;
                }
            } else if(sigroam_scene_upload_sd_dead(app) || sigroam_scene_upload_hold(app)) {
                app->upload_go_retry = false;
            } else if(sr_worker_send_cmd(app->worker, sr_upload_go_cmd(false))) {
                app->upload_go_retry = false;
            }
        } else {
            sigroam_scene_upload_poll(app);
        }
        if(app->upload_up_rev_shown != app->model.up_rev ||
           app->upload_fw_rev_shown != app->model.firmware_rev ||
           app->upload_qual_rev_shown != app->model.qual_rev ||
           app->upload_cfg_rev_shown != app->model.cfg_rev ||
           (generic && s_mau_rev_shown != sr_mau_up_rev()) ||
           (generic && s_logs_rev != sr_mau_logs_rev()) || (app->tick_n % 10u) == 0u) {
            s_mau_rev_shown = sr_mau_up_rev();
            app->upload_up_rev_shown = app->model.up_rev;
            app->upload_fw_rev_shown = app->model.firmware_rev;
            app->upload_qual_rev_shown = app->model.qual_rev;
            app->upload_cfg_rev_shown = app->model.cfg_rev;
            s_logs_rev = sr_mau_logs_rev();
            sigroam_scene_upload_draw(app, false);
        }
        return true;
    }
    return false;
}

void sigroam_scene_upload_on_exit(void* context) {
    SigRoamApp* app = context;
    s_page = UploadPageResult;
    s_block = SrMauLogsBlockNone;
    sr_mau_logs_reset();
    widget_reset(app->widget);
}
