// SPDX-License-Identifier: GPL-3.0
// Copyright (C) 2026 RogerF5. Standalone USB policy validation.
#include "../vendor/mass_storage_usb.h"
#include "pc_font.h"
#include "pc_i18n.h"
#include "pc_settings.h"
#include "probe_disk.h"
#include "probe_status.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdio.h>
#include <storage/storage.h>
#include <string.h>
#define VERSION    "0.3"
#define LAST_MAGIC 0x50433301u
#define T(a, key)  pc_text((a)->cfg.language, (key))
static const char* ids[] = {"hid", "msc_rw", "msc_ro"};
static const PcText mode_text[] = {TxKeyboard, TxDiskRw, TxDiskRo};
static const PcText signal_text[] =
    {TxNoResponse, TxHidConfigured, TxHidSent, TxDiskRead, TxDiskWritten, TxIoError};
static const PcText outcome_text[] = {TxInconclusive, TxAllowed, TxBlocked};
static const PcText return_text[] = {TxReturnNone, TxReturnOk, TxReturnChanged, TxReturnError};
typedef enum {
    ScreenSplash,
    ScreenMenu,
    ScreenSettings,
    ScreenHelp,
    ScreenPrepare,
    ScreenActive,
    ScreenResult,
    ScreenHistory
} Screen;
typedef struct {
    uint32_t magic, rtc, reads, writes, crc;
    uint8_t mode, signal, outcome, returned, probe_mask;
    char token[PC_TOKEN_SIZE];
} LastReport;
typedef struct {
    Gui* gui;
    ViewPort* view;
    FuriMessageQueue* keys;
    FuriMutex* mutex;
    Storage* storage;
    File* log;
    File* disk;
    MassStorageUsb* mass;
    FuriHalUsbInterface* previous;
    PcMode profile;
    PcSettings cfg;
    Screen screen;
    uint8_t menu, setting, outcome, pos;
    volatile uint8_t probe_mask;
    bool active, logging_error, transport_seen, sent, timed_out, sending, release_phase,
        restore_error, last_valid, details;
    uint32_t next_key, start_tick, arm_tick, seq, last_poll, last_read, last_write;
    volatile uint32_t reads, writes, io_errors;
    PcReturnResult returned;
    uint32_t return_crc;
    char token[PC_TOKEN_SIZE], logfile[128];
    PcText message;
    LastReport last;
} App;
static uint32_t elapsed(App* a) {
    return (uint32_t)((uint64_t)(furi_get_tick() - a->start_tick) * 1000 /
                      furi_kernel_get_tick_frequency());
}
static PcSignal signal_now(App* a) {
    return pc_observation(
        a->profile, a->transport_seen, a->sent, a->reads, a->writes, a->io_errors);
}
static unsigned probe_count(uint8_t mask) {
    unsigned n = 0;
    for(unsigned i = 0; i < 8; i++)
        n += (mask >> i) & 1;
    return n;
}
static void event(App* a, const char* kind, const char* detail) {
    if(!a->log) return;
    char line[448];
    int n = snprintf(
        line,
        sizeof(line),
        "{\"schema\":1,\"app_version\":\"0.3\",\"session\":\"%s\","
        "\"seq\":%lu,\"rtc_local\":%lu,\"elapsed_ms\":%lu,"
        "\"profile\":\"%s\",\"event\":\"%s\",\"detail\":\"%s\"}\n",
        a->token,
        (unsigned long)++a->seq,
        (unsigned long)furi_hal_rtc_get_timestamp(),
        (unsigned long)elapsed(a),
        ids[a->profile],
        kind,
        detail);
    if(n < 0 || n >= (int)sizeof(line) || storage_file_write(a->log, line, n) != (uint16_t)n ||
       !storage_file_sync(a->log))
        a->logging_error = true;
}
static void settings_load(App* a) {
    pc_settings_default(&a->cfg);
    uint8_t bytes[PC_SETTINGS_BYTES];
    File* f = storage_file_alloc(a->storage);
    if(storage_file_open(f, APP_DATA_PATH("settings.bin"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(f) == sizeof(bytes) &&
           storage_file_read(f, bytes, sizeof(bytes)) == sizeof(bytes))
            pc_settings_decode(&a->cfg, bytes, sizeof(bytes));
        storage_file_close(f);
    }
    storage_file_free(f);
}
static void settings_save(App* a) {
    storage_common_mkdir(a->storage, APP_DATA_PATH(""));
    uint8_t bytes[PC_SETTINGS_BYTES];
    pc_settings_encode(&a->cfg, bytes);
    File* f = storage_file_alloc(a->storage);
    bool ok = storage_file_open(f, APP_DATA_PATH("settings.bin"), FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok)
        ok = storage_file_write(f, bytes, sizeof(bytes)) == sizeof(bytes) && storage_file_sync(f);
    storage_file_close(f);
    storage_file_free(f);
    a->message = ok ? TxSaved : TxSdError;
}
static bool
    disk_read(void* ctx, uint32_t lba, uint16_t count, uint8_t* out, uint32_t* len, uint32_t cap) {
    App* a = ctx;
    *len = 0;
    if(lba >= PC_BLOCKS || count > PC_BLOCKS - lba || cap < 512) {
        a->io_errors++;
        return false;
    }
    uint32_t n = MIN((uint32_t)count, cap / 512) * 512;
    if(!n) return true;
    if(!storage_file_seek(a->disk, lba * 512, true) || storage_file_read(a->disk, out, n) != n) {
        a->io_errors++;
        return false;
    }
    *len = n;
    if(a->cfg.probe_files)
        for(uint32_t block = lba; block < lba + n / 512; block++)
            if(block >= PC_SEND_START && block < PC_SEND_START + PC_SEND_BLOCKS)
                a->probe_mask |= 1u << (block - PC_SEND_START);
    a->reads += n / 512;
    return true;
}
static bool disk_write(void* ctx, uint32_t lba, uint16_t count, uint8_t* data, uint32_t len) {
    App* a = ctx;
    if(a->profile == PcDiskRo || lba >= PC_BLOCKS || count > PC_BLOCKS - lba ||
       len != (uint32_t)count * 512) {
        a->io_errors++;
        return false;
    }
    if(!storage_file_seek(a->disk, lba * 512, true) ||
       storage_file_write(a->disk, data, len) != len) {
        a->io_errors++;
        return false;
    }
    a->writes += count;
    return true;
}
static uint32_t disk_blocks(void* ctx) {
    UNUSED(ctx);
    return PC_BLOCKS;
}
static void disk_eject(void* ctx) {
    UNUSED(ctx);
}
static bool disk_prepare(App* a) {
    a->disk = storage_file_alloc(a->storage);
    if(!storage_file_open(a->disk, APP_DATA_PATH("probe.img"), FSAM_READ_WRITE, FSOM_CREATE_ALWAYS))
        return false;
    uint8_t sector[512];
    for(uint32_t i = 0; i < PC_BLOCKS; i++) {
        pc_disk_sector_config(i, a->token, a->cfg.probe_files, sector);
        if(storage_file_write(a->disk, sector, 512) != 512) return false;
    }
    return storage_file_sync(a->disk);
}

static bool read_image(void* ctx, uint32_t lba, uint8_t out[512]) {
    App* a = ctx;
    return lba < PC_BLOCKS && storage_file_seek(a->disk, lba * 512, true) &&
           storage_file_read(a->disk, out, 512) == 512;
}
static void last_load(App* a) {
    File* f = storage_file_alloc(a->storage);
    if(storage_file_open(f, APP_DATA_PATH("last.bin"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        bool ok = storage_file_size(f) == sizeof(a->last) &&
                  storage_file_read(f, &a->last, sizeof(a->last)) == sizeof(a->last);
        a->last.token[PC_TOKEN_SIZE - 1] = 0;
        a->last_valid = ok && a->last.magic == LAST_MAGIC && a->last.mode < PcModeCount &&
                        a->last.signal < PcSignalCount && a->last.outcome < 3 &&
                        a->last.returned <= PcReturnIoError && pc_token_valid(a->last.token);
        storage_file_close(f);
    }
    storage_file_free(f);
}
static void last_save(App* a) {
    memset(&a->last, 0, sizeof(a->last));
    a->last.magic = LAST_MAGIC;
    a->last.rtc = furi_hal_rtc_get_timestamp();
    a->last.mode = a->profile;
    a->last.signal = signal_now(a);
    a->last.outcome = a->outcome;
    a->last.returned = a->returned;
    a->last.reads = a->reads;
    a->last.writes = a->writes;
    a->last.probe_mask = a->probe_mask;
    a->last.crc = a->return_crc;
    memcpy(a->last.token, a->token, sizeof(a->token));
    File* f = storage_file_alloc(a->storage);
    bool ok = storage_file_open(f, APP_DATA_PATH("last.bin"), FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok)
        ok = storage_file_write(f, &a->last, sizeof(a->last)) == sizeof(a->last) &&
             storage_file_sync(f);
    storage_file_close(f);
    storage_file_free(f);
    a->last_valid = ok;
    if(!ok) a->logging_error = true;
}
static void finish(App* a, const char* reason) {
    if(!a->active) return;
    a->sending = false;
    if(a->profile == PcKeyboard) furi_hal_hid_kb_release_all();
    bool ok;
    if(a->mass) {
        ok = mass_storage_usb_stop(a->mass);
        if(ok) a->mass = NULL;
    } else
        ok = furi_hal_usb_set_config(a->previous, NULL);
    if(!ok) {
        a->restore_error = true;
        event(a, "usb_restore_error", "USB_busy_Back_to_retry");
        return;
    }
    if(a->disk) {
        if(!storage_file_sync(a->disk)) a->io_errors++;
        if(a->cfg.probe_files)
            a->returned = pc_verify_return(read_image, a, a->token, &a->return_crc);
        storage_file_close(a->disk);
        storage_file_free(a->disk);
        a->disk = NULL;
    }
    char detail[128];
    snprintf(
        detail,
        sizeof(detail),
        "read_%lu_write_%lu_errors_%lu_probe_blocks_%u_return_%u_crc32_%08lX",
        (unsigned long)a->reads,
        (unsigned long)a->writes,
        (unsigned long)a->io_errors,
        probe_count(a->probe_mask),
        a->returned,
        (unsigned long)a->return_crc);
    event(a, "final_counters", detail);
    if(a->returned == PcReturnMatch)
        event(
            a,
            "synthetic_return_matched",
            "RETURN_TXT_4096_bytes_exact_session_pattern_no_private_data");
    event(a, "trial_stopped", reason);
    a->active = false;
    a->screen = ScreenResult;
    a->restore_error = false;
    a->details = false;
}
static bool begin(App* a) {
    if(furi_hal_usb_is_locked()) {
        a->message = TxCloseQ;
        return false;
    }
    if(furi_hal_usb_get_config() != &usb_cdc_single) {
        a->message = TxCloseOther;
        return false;
    }
    a->previous = furi_hal_usb_get_config();
    a->start_tick = furi_get_tick();
    a->seq = 0;
    a->transport_seen = a->sent = a->timed_out = a->sending = a->restore_error = a->logging_error =
        a->details = false;
    a->reads = a->writes = a->io_errors = a->last_read = a->last_write = a->probe_mask = 0;
    a->last_poll = furi_get_tick();
    a->outcome = 0;
    a->returned = PcReturnNone;
    a->return_crc = 0;
    snprintf(
        a->token,
        sizeof(a->token),
        "PC%08lX%08lX",
        (unsigned long)furi_hal_random_get(),
        (unsigned long)furi_hal_random_get());
    storage_common_mkdir(a->storage, APP_DATA_PATH(""));
    storage_common_mkdir(a->storage, APP_DATA_PATH("sessions"));
    snprintf(a->logfile, sizeof(a->logfile), APP_DATA_PATH("sessions/%s.jsonl"), a->token);
    a->log = storage_file_alloc(a->storage);
    if(!storage_file_open(a->log, a->logfile, FSAM_WRITE, FSOM_CREATE_NEW)) {
        storage_file_free(a->log);
        a->log = NULL;
        a->message = TxSdError;
        return false;
    }
    event(a, "trial_started", "manual_start_fixed_USB_identity_RTC_local");
    bool ok = false;
    if(!a->logging_error) {
        if(a->profile == PcKeyboard)
            ok = furi_hal_usb_set_config(&usb_hid, NULL);
        else if(disk_prepare(a)) {
            SCSIDeviceFunc fn = {
                .ctx = a,
                .read_only = a->profile == PcDiskRo,
                .read = disk_read,
                .write = disk_write,
                .num_blocks = disk_blocks,
                .eject = disk_eject};
            a->mass = mass_storage_usb_start(a->token, fn);
            ok = a->mass != NULL;
        }
    }
    if(!ok) {
        if(a->disk) {
            storage_file_close(a->disk);
            storage_file_free(a->disk);
            a->disk = NULL;
        }
        event(a, "start_error", "USB_profile_or_SD_failed");
        storage_file_close(a->log);
        storage_file_free(a->log);
        a->log = NULL;
        a->message = TxStartError;
        return false;
    }
    a->active = true;
    a->screen = ScreenActive;
    a->message = TxCount;
    return true;
}
static void poll(App* a) {
    if(a->screen == ScreenPrepare && furi_get_tick() - a->arm_tick >= furi_ms_to_ticks(8000)) {
        a->screen = ScreenMenu;
        begin(a);
    }
    if(!a->active) return;
    bool seen = a->profile == PcKeyboard ? furi_hal_hid_is_connected() :
                                           a->reads > 0 || a->writes > 0;
    if(seen && !a->transport_seen) {
        a->transport_seen = true;
        event(a, "transport_observed", "USB_activity_not_policy_verdict");
    }
    if(a->sending && (int32_t)(furi_get_tick() - a->next_key) >= 0) {
        if(!furi_hal_hid_is_connected()) {
            a->sending = false;
            furi_hal_hid_kb_release_all();
            event(a, "hid_send_failed", "USB_disconnected");
        } else if(a->release_phase) {
            bool ok = furi_hal_hid_kb_release_all();
            a->release_phase = false;
            if(!ok) {
                a->sending = false;
                event(a, "hid_send_failed", "release_report_failed");
            } else if(++a->pos >= strlen(a->token)) {
                a->sending = false;
                a->sent = true;
                event(a, "hid_reports_sent", "marker_only_operator_confirmation_required");
            }
            a->next_key = furi_get_tick() + furi_ms_to_ticks(40);
        } else {
            bool ok = furi_hal_hid_kb_press(HID_ASCII_TO_KEY(a->token[a->pos]));
            if(!ok) {
                a->sending = false;
                furi_hal_hid_kb_release_all();
                event(a, "hid_send_failed", "press_report_failed");
            } else
                a->release_phase = true;
            a->next_key = furi_get_tick() + furi_ms_to_ticks(40);
        }
    }
    if(elapsed(a) >= pc_duration_ms(&a->cfg) && !a->timed_out) {
        a->timed_out = true;
        finish(a, "time_limit_not_policy_block_confirmation");
    }
    if(a->logging_error && !a->restore_error) {
        finish(a, "SD_logging_failed");
        return;
    }
    if(furi_get_tick() - a->last_poll >= furi_ms_to_ticks(1000)) {
        a->last_poll = furi_get_tick();
        if(a->reads != a->last_read || a->writes != a->last_write) {
            char detail[96];
            snprintf(
                detail,
                sizeof(detail),
                "read_%lu_write_%lu_probe_blocks_%u",
                (unsigned long)a->reads,
                (unsigned long)a->writes,
                probe_count(a->probe_mask));
            event(a, "storage_io", detail);
            a->last_read = a->reads;
            a->last_write = a->writes;
        }
    }
}
static void text(App* a, Canvas* c, int y, PcText key) {
    pc_draw_text(c, 0, y, T(a, key));
}
static void header(Canvas* c, const char* title) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 0, 9, title);
    canvas_draw_line(c, 0, 12, 127, 12);
    canvas_set_font(c, FontSecondary);
}
static void splash(App* a, Canvas* c) {
    canvas_draw_line(c, 3, 4, 30, 4);
    canvas_draw_line(c, 30, 4, 30, 24);
    canvas_draw_line(c, 30, 24, 17, 35);
    canvas_draw_line(c, 17, 35, 3, 24);
    canvas_draw_line(c, 3, 24, 3, 4);
    canvas_draw_line(c, 17, 28, 17, 10);
    canvas_draw_line(c, 17, 10, 13, 14);
    canvas_draw_line(c, 17, 10, 21, 14);
    canvas_draw_line(c, 17, 23, 9, 19);
    canvas_draw_disc(c, 8, 17, 2);
    canvas_draw_line(c, 17, 20, 24, 16);
    canvas_draw_box(c, 23, 11, 3, 4);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 39, 17, "PURPLE");
    canvas_draw_str(c, 39, 31, "CHECK");
    canvas_set_font(c, FontSecondary);
    canvas_draw_line(c, 0, 40, 127, 40);
    pc_draw_text(c, 4, 50, "USB CONTROL VALIDATOR");
    pc_draw_text(c, 4, 62, "RogerF5");
    pc_draw_text(c, 83, 62, "v0.3  OK");
    UNUSED(a);
}
static void draw(App* a, Canvas* c) {
    canvas_clear(c);
    if(a->screen == ScreenSplash) {
        splash(a, c);
        return;
    }
    header(c, "PurpleCheck");
    char line[64];
    if(a->screen == ScreenMenu) {
        static const PcText entries[] = {TxDiskRw, TxDiskRo, TxKeyboard, TxLast, TxSettings};
        unsigned first = a->menu > 2 ? a->menu - 2 : 0;
        for(unsigned row = 0; row < 3 && first + row < 5; row++) {
            unsigned index = first + row;
            if(index == a->menu) {
                canvas_draw_box(c, 0, 15 + row * 12, 128, 12);
                canvas_set_color(c, ColorWhite);
            }
            pc_draw_text(c, 3, 24 + row * 12, T(a, entries[index]));
            canvas_set_color(c, ColorBlack);
        }
        text(a, c, 62, a->message < TxCount ? a->message : TxSelect);
    } else if(a->screen == ScreenSettings) {
        static const PcText labels[] = {TxLanguage, TxDuration, TxProbeFiles, TxAuthor, TxVersion};
        unsigned first = a->setting > 2 ? a->setting - 2 : 0;
        for(unsigned row = 0; row < 3 && first + row < 5; row++) {
            unsigned index = first + row;
            if(index == a->setting) {
                canvas_draw_box(c, 0, 15 + row * 12, 128, 12);
                canvas_set_color(c, ColorWhite);
            }
            pc_draw_text(c, 2, 23 + row * 12, T(a, labels[index]));
            const char* value = "";
            if(index == 0)
                value = a->cfg.language == PcEnglish ? "EN" :
                        a->cfg.language == PcSpanish ? "ES" :
                                                       "UK";
            if(index == 1) {
                snprintf(
                    line, sizeof(line), "%lus", (unsigned long)(pc_duration_ms(&a->cfg) / 1000));
                value = line;
            }
            if(index == 2) value = T(a, a->cfg.probe_files ? TxOn : TxOff);
            if(index == 3) value = "RogerF5";
            if(index == 4) value = VERSION;
            pc_draw_text(c, index == 3 ? 78 : 103, 23 + row * 12, value);
            canvas_set_color(c, ColorBlack);
        }
        text(a, c, 62, TxSave);
    } else if(a->screen == ScreenHelp) {
        text(a, c, 23, TxHelp1);
        text(a, c, 35, TxHelp2);
        text(a, c, 47, TxHelp3);
        text(a, c, 62, TxHelp4);
    } else if(a->screen == ScreenHistory) {
        if(a->last_valid) {
            text(a, c, 23, mode_text[a->last.mode]);
            text(
                a, c, 35, a->details ? return_text[a->last.returned] : signal_text[a->last.signal]);
            text(a, c, 47, outcome_text[a->last.outcome]);
            if(a->details) {
                snprintf(line, sizeof(line), "CRC32: %08lX", (unsigned long)a->last.crc);
                pc_draw_text(c, 0, 62, line);
            } else
                pc_draw_text(c, 0, 62, a->last.token);
        } else
            text(a, c, 35, TxNoHistory);
    } else if(a->screen == ScreenResult) {
        text(a, c, 23, a->details ? return_text[a->returned] : signal_text[signal_now(a)]);
        text(a, c, 35, outcome_text[a->outcome]);
        text(a, c, 47, TxResultHint);
        text(a, c, 62, a->logging_error ? TxSdError : TxManual);
    } else if(a->screen == ScreenPrepare) {
        text(a, c, 23, mode_text[a->profile]);
        unsigned seconds = (unsigned)((uint64_t)(furi_get_tick() - a->arm_tick) /
                                      furi_kernel_get_tick_frequency());
        snprintf(line, sizeof(line), T(a, TxStarting), (unsigned long)(8u - MIN(8u, seconds)));
        pc_draw_text(c, 0, 35, line);
        text(a, c, 47, TxCloseQ);
        text(a, c, 62, TxCancel);
    } else if(a->screen == ScreenActive) {
        snprintf(
            line,
            sizeof(line),
            "%lu / %lus",
            (unsigned long)(elapsed(a) / 1000),
            (unsigned long)(pc_duration_ms(&a->cfg) / 1000));
        pc_draw_text(c, 0, 23, line);
        canvas_draw_frame(c, 0, 25, 127, 4);
        unsigned width =
            MIN(125u, (unsigned)((uint64_t)elapsed(a) * 125 / pc_duration_ms(&a->cfg)));
        if(width) canvas_draw_box(c, 1, 26, width, 2);
        if(a->details && a->profile != PcKeyboard) {
            snprintf(line, sizeof(line), "SEND.TXT: %u/8", probe_count(a->probe_mask));
            pc_draw_text(c, 0, 40, line);
            text(a, c, 51, TxHelp3);
        } else {
            text(a, c, 40, signal_text[signal_now(a)]);
            if(a->profile == PcKeyboard)
                text(a, c, 51, a->sending ? TxSending : a->sent ? TxCheckText : TxSend);
            else {
                snprintf(
                    line,
                    sizeof(line),
                    T(a, TxCounters),
                    (unsigned long)a->reads,
                    (unsigned long)a->writes);
                pc_draw_text(c, 0, 51, line);
            }
        }
        text(a, c, 63, a->restore_error ? TxCloseOther : TxStop);
    }
}
static void draw_cb(Canvas* c, void* ctx) {
    App* a = ctx;
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    draw(a, c);
    furi_mutex_release(a->mutex);
}
static void input(InputEvent* e, void* ctx) {
    App* a = ctx;
    if(e->type == InputTypeShort || e->type == InputTypeLong)
        furi_message_queue_put(a->keys, e, 0);
}
static void save_result(App* a) {
    static const char* values[] = {
        "inconclusive", "allowed_operator_observed", "blocked_operator_observed"};
    event(a, "operator_outcome", values[a->outcome]);
    event(a, "session_closed", "USB_observations_and_manual_outcome_are_separate");
    last_save(a);
    if(a->log) {
        storage_file_close(a->log);
        storage_file_free(a->log);
        a->log = NULL;
    }
    a->screen = ScreenMenu;
    a->message = a->logging_error ? TxSdError : TxSaved;
}
static void settings_key(App* a, InputKey key) {
    if(key == InputKeyBack) {
        settings_save(a);
        a->screen = ScreenMenu;
        return;
    }
    if(key == InputKeyUp) a->setting = (a->setting + 4) % 5;
    if(key == InputKeyDown) a->setting = (a->setting + 1) % 5;
    if(key == InputKeyLeft || key == InputKeyRight || key == InputKeyOk) {
        unsigned delta = key == InputKeyLeft ? 2 : 1;
        if(a->setting == 0)
            a->cfg.language = (a->cfg.language + delta) % PcLanguageCount;
        else if(a->setting == 1)
            a->cfg.duration = (a->cfg.duration + delta) % 3;
        else if(a->setting == 2)
            a->cfg.probe_files = !a->cfg.probe_files;
        else if(key == InputKeyOk) {
            settings_save(a);
            a->screen = ScreenMenu;
        }
    }
}
int32_t purplecheck_app(void* argument) {
    UNUSED(argument);
    App* a = calloc(1, sizeof(App));
    furi_check(a);
    a->message = TxCount;
    a->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    a->keys = furi_message_queue_alloc(8, sizeof(InputEvent));
    a->storage = furi_record_open(RECORD_STORAGE);
    settings_load(a);
    last_load(a);
    a->gui = furi_record_open(RECORD_GUI);
    a->view = view_port_alloc();
    view_port_draw_callback_set(a->view, draw_cb, a);
    view_port_input_callback_set(a->view, input, a);
    gui_add_view_port(a->gui, a->view, GuiLayerFullscreen);
    bool running = true;
    while(running) {
        InputEvent e;
        bool got = furi_message_queue_get(a->keys, &e, 20) == FuriStatusOk;
        furi_mutex_acquire(a->mutex, FuriWaitForever);
        if(got && (e.type == InputTypeShort || e.key == InputKeyBack)) {
            InputKey key = e.key;
            if(a->screen == ScreenSplash) {
                if(key == InputKeyBack)
                    running = false;
                else
                    a->screen = ScreenMenu;
            } else if(a->screen == ScreenSettings)
                settings_key(a, key);
            else if(a->screen == ScreenHelp) {
                if(key == InputKeyBack || key == InputKeyOk) a->screen = ScreenMenu;
            } else if(a->screen == ScreenHistory) {
                if(key == InputKeyBack || key == InputKeyOk)
                    a->screen = ScreenMenu;
                else if(key == InputKeyLeft || key == InputKeyRight)
                    a->details = !a->details;
            } else if(a->screen == ScreenResult) {
                if(key == InputKeyLeft) a->outcome = (a->outcome + 2) % 3;
                if(key == InputKeyRight) a->outcome = (a->outcome + 1) % 3;
                if(key == InputKeyUp || key == InputKeyDown) a->details = !a->details;
                if(key == InputKeyOk || key == InputKeyBack) save_result(a);
            } else if(a->screen == ScreenPrepare) {
                if(key == InputKeyBack) a->screen = ScreenMenu;
            } else if(a->screen == ScreenActive) {
                if(key == InputKeyBack || (key == InputKeyOk && a->profile != PcKeyboard))
                    finish(a, "operator_stop");
                else if(key == InputKeyLeft || key == InputKeyRight)
                    a->details = !a->details;
                else if(key == InputKeyOk && !a->sending && !a->sent && furi_hal_hid_is_connected()) {
                    a->sending = true;
                    a->pos = 0;
                    a->release_phase = false;
                    a->next_key = furi_get_tick() + furi_ms_to_ticks(1000);
                    event(a, "hid_marker_armed", "manual_OK_one_second_delay");
                }
            } else {
                if(key == InputKeyBack) running = false;
                if(key == InputKeyUp) a->menu = (a->menu + 4) % 5;
                if(key == InputKeyDown) a->menu = (a->menu + 1) % 5;
                if(key == InputKeyRight) {
                    a->screen = ScreenSettings;
                    a->setting = 0;
                }
                if(key == InputKeyLeft) a->screen = ScreenHelp;
                if(key == InputKeyOk) {
                    if(a->menu < 3) {
                        static const PcMode modes[] = {PcDiskRw, PcDiskRo, PcKeyboard};
                        a->profile = modes[a->menu];
                        a->arm_tick = furi_get_tick();
                        a->screen = ScreenPrepare;
                    } else if(a->menu == 3) {
                        a->screen = ScreenHistory;
                        a->details = false;
                    } else {
                        a->screen = ScreenSettings;
                        a->setting = 0;
                    }
                }
            }
        }
        poll(a);
        furi_mutex_release(a->mutex);
        view_port_update(a->view);
    }
    view_port_enabled_set(a->view, false);
    gui_remove_view_port(a->gui, a->view);
    view_port_free(a->view);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(a->keys);
    furi_mutex_free(a->mutex);
    free(a);
    return 0;
}
