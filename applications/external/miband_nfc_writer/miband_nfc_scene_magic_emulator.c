/**
 * @file miband_nfc_scene_magic_emulator.c
 * @brief Magic card emulation scene - FIXED VERSION
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

typedef struct {
    uint32_t auth_attempts;
    uint32_t successful_auths;
    uint32_t failed_auths;
    uint32_t reader_connections;
    uint32_t data_reads;
    FuriString* last_activity;
    FuriString* status_message;
    bool is_active;
    FuriTimer* update_timer;
    FuriMutex* mutex;
} EmulationStats;

static void emulation_stats_free(EmulationStats* stats) {
    if(!stats) return;

    if(stats->update_timer) {
        furi_timer_stop(stats->update_timer);
        furi_timer_free(stats->update_timer);
        stats->update_timer = NULL;
    }

    // FIX: Always mark inactive and free strings, release mutex before freeing it
    if(stats->mutex) {
        if(furi_mutex_acquire(stats->mutex, 200) == FuriStatusOk) {
            stats->is_active = false;
            furi_mutex_release(stats->mutex);
        }
    }

    // Free strings outside mutex - timer is already stopped so no concurrent access
    if(stats->last_activity) {
        furi_string_free(stats->last_activity);
        stats->last_activity = NULL;
    }

    if(stats->status_message) {
        furi_string_free(stats->status_message);
        stats->status_message = NULL;
    }

    if(stats->mutex) {
        furi_mutex_free(stats->mutex);
        stats->mutex = NULL;
    }

    free(stats);
}

static void emulation_timer_callback(void* context) {
    if(!context) return;

    MiBandNfcApp* app = (MiBandNfcApp*)context;

    if(!app || !app->popup || !app->view_dispatcher) {
        return;
    }

    if(!app->is_emulating) {
        return;
    }

    if(!app->emulation_stats) {
        return;
    }

    EmulationStats* stats = (EmulationStats*)app->emulation_stats;

    if(!stats || !stats->mutex) {
        return;
    }

    if(furi_mutex_acquire(stats->mutex, 50) != FuriStatusOk) {
        return;
    }

    if(!stats->is_active) {
        furi_mutex_release(stats->mutex);
        return;
    }

    // FIX: Build the entire display string while holding the mutex to avoid
    // race condition where string pointers become stale after release.
    // Use app->temp_text_buffer (persistent) because popup_set_text stores
    // pointer only - a local FuriString freed after set would be use-after-free.
    furi_string_printf(app->temp_text_buffer, "UID+0xFF template\n");
    furi_string_cat_printf(
        app->temp_text_buffer,
        "Auth: %lu | OK %lu\n",
        stats->auth_attempts,
        stats->successful_auths);

    if(stats->last_activity && furi_string_size(stats->last_activity) > 0) {
        furi_string_cat_printf(
            app->temp_text_buffer, "%s\n", furi_string_get_cstr(stats->last_activity));
    }

    if(stats->status_message && furi_string_size(stats->status_message) > 0) {
        furi_string_cat_printf(
            app->temp_text_buffer, "%s\n", furi_string_get_cstr(stats->status_message));
    }

    furi_string_cat_str(app->temp_text_buffer, "Back=Stop");

    furi_mutex_release(stats->mutex);

    popup_reset(app->popup);
    popup_set_header(app->popup, "Magic Template Active", 64, 2, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, furi_string_get_cstr(app->temp_text_buffer), 2, 12, AlignLeft, AlignTop);
    popup_set_icon(app->popup, 68, 20, &I_NFC_dolphin_emulation_51x64);
}

static EmulationStats* emulation_stats_alloc(MiBandNfcApp* app) {
    EmulationStats* stats = malloc(sizeof(EmulationStats));
    if(!stats) return NULL;

    memset(stats, 0, sizeof(EmulationStats));

    stats->auth_attempts = 0;
    stats->successful_auths = 0;
    stats->failed_auths = 0;
    stats->reader_connections = 0;
    stats->data_reads = 0;
    stats->last_activity = furi_string_alloc();
    stats->status_message = furi_string_alloc();
    stats->is_active = true;
    stats->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    if(!stats->last_activity || !stats->status_message || !stats->mutex) {
        FURI_LOG_E(TAG, "Failed to allocate resources");
        emulation_stats_free(stats);
        return NULL;
    }

    stats->update_timer = furi_timer_alloc(emulation_timer_callback, FuriTimerTypePeriodic, app);
    if(!stats->update_timer) {
        FURI_LOG_E(TAG, "Failed to allocate timer");
        emulation_stats_free(stats);
        return NULL;
    }

    furi_timer_start(stats->update_timer, 1000);

    furi_string_set_str(stats->status_message, "Template ready");
    furi_string_set_str(stats->last_activity, "Emulation started");

    return stats;
}

static void miband_nfc_magic_emulator_prepare_blank_template(MiBandNfcApp* app) {
    if(!app || !app->is_valid_nfc_data) {
        return;
    }

    EmulationStats* stats = (EmulationStats*)app->emulation_stats;
    if(stats) {
        if(furi_mutex_acquire(stats->mutex, 100) == FuriStatusOk) {
            furi_string_set_str(stats->status_message, "Preparing template...");
            furi_string_set_str(stats->last_activity, "Template generation");
            furi_mutex_release(stats->mutex);
        }
    }

    uint8_t original_block0_data[16];
    memcpy(original_block0_data, app->mf_classic_data->block[0].data, 16);

    uint8_t uid[4];
    memcpy(uid, app->mf_classic_data->block[0].data, 4);

    uint8_t bcc = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];

    size_t total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);

    for(size_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        memset(app->mf_classic_data->block[block_idx].data, 0x00, 16);
    }

    memcpy(app->mf_classic_data->block[0].data, uid, 4);
    app->mf_classic_data->block[0].data[4] = bcc;
    app->mf_classic_data->block[0].data[5] = 0x08;
    app->mf_classic_data->block[0].data[6] = 0x04;
    app->mf_classic_data->block[0].data[7] = 0x00;
    memcpy(&app->mf_classic_data->block[0].data[8], &original_block0_data[8], 8);

    for(size_t sector_idx = 0; sector_idx < total_sectors; sector_idx++) {
        size_t sector_trailer_block_idx;
        if(sector_idx < 32) {
            sector_trailer_block_idx = sector_idx * 4 + 3;
        } else {
            sector_trailer_block_idx = 32 * 4 + (sector_idx - 32) * 16 + 15;
        }

        MfClassicBlock* sector_trailer = &app->mf_classic_data->block[sector_trailer_block_idx];

        /* Construct a safe trailer. Note: access bits ideally should be calculated to be consistent */
        memset(&sector_trailer->data[0], 0xFF, 6); /* Key A = FF..FF */
        sector_trailer->data[6] = 0xFF; /* access byte 1 (example) */
        sector_trailer->data[7] = 0x07; /* access byte 2 (example) */
        sector_trailer->data[8] = 0x80; /* access byte 3 (example) */
        sector_trailer->data[9] = 0x69; /* Key B first byte placeholder (if any) */
        memset(&sector_trailer->data[10], 0xFF, 6); /* Key B = FF..FF */

        FURI_BIT_SET(app->mf_classic_data->key_a_mask, sector_idx);
        FURI_BIT_SET(app->mf_classic_data->key_b_mask, sector_idx);
    }

    if(stats) {
        if(furi_mutex_acquire(stats->mutex, 100) == FuriStatusOk) {
            furi_string_printf(
                stats->status_message, "UID %02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
            furi_string_set_str(stats->last_activity, "Template ready");
            furi_mutex_release(stats->mutex);
        }
    }

    FURI_LOG_I(
        TAG,
        "Prepared blank Mi Band template - UID: %02X%02X%02X%02X",
        uid[0],
        uid[1],
        uid[2],
        uid[3]);
}

static NfcCommand miband_nfc_magic_emulator_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(!app || !app->emulation_stats) {
        return NfcCommandStop;
    }

    EmulationStats* stats = (EmulationStats*)app->emulation_stats;

    if(!stats->mutex) {
        return NfcCommandStop;
    }

    if(furi_mutex_acquire(stats->mutex, 100) != FuriStatusOk) {
        return NfcCommandContinue;
    }

    bool should_stop = false;

    if(!stats->is_active) {
        should_stop = true;
    } else if(event.protocol == NfcProtocolMfClassic) {
        const MfClassicListenerEvent* mfc_event = event.event_data;

        if(mfc_event->type == MfClassicListenerEventTypeAuthContextPartCollected) {
            if(stats->last_activity) {
                furi_string_set_str(stats->last_activity, "Auth data collected");
            }
            FURI_LOG_D(TAG, "Blank template: Auth part collected");

        } else if(mfc_event->type == MfClassicListenerEventTypeAuthContextFullCollected) {
            stats->auth_attempts++;
            stats->successful_auths++;

            if(stats->last_activity) {
                furi_string_printf(
                    stats->last_activity, "Auth #%lu complete", stats->auth_attempts);
            }

            if(stats->status_message) {
                furi_string_set_str(stats->status_message, "Authentication successful");
            }

            FURI_LOG_D(TAG, "Blank template: Auth complete #%lu", stats->auth_attempts);

            if(stats->auth_attempts % 5 == 0) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MiBandNfcCustomEventCardDetected);
            }

            if(app->logger && stats->auth_attempts % 10 == 0) {
                miband_logger_log(
                    app->logger,
                    LogLevelDebug,
                    "Magic emulation: %lu authentications completed",
                    stats->auth_attempts);
            }

            if(stats->auth_attempts > 100) {
                if(stats->status_message) {
                    furi_string_set_str(
                        stats->status_message, "Too many auth attempts - stopping");
                }
                if(stats->last_activity) {
                    furi_string_set_str(stats->last_activity, "Safety stop triggered");
                }

                FURI_LOG_W(TAG, "Too many auth attempts (%lu), stopping", stats->auth_attempts);

                if(app->logger) {
                    miband_logger_log(
                        app->logger,
                        LogLevelWarning,
                        "Magic emulation stopped: too many auth attempts (%lu)",
                        stats->auth_attempts);
                }

                stats->is_active = false;
                should_stop = true;
            }
        }
    }

    furi_mutex_release(stats->mutex);

    return should_stop ? NfcCommandStop : NfcCommandContinue;
}

void miband_nfc_scene_magic_emulator_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    app->is_emulating = true;

    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Magic emulation started");
    }

    if(!app->is_valid_nfc_data) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    app->emulation_stats = emulation_stats_alloc(app);
    if(!app->emulation_stats) {
        FURI_LOG_E(TAG, "Failed to allocate emulation stats");
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    miband_nfc_magic_emulator_prepare_blank_template(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdMagicEmulator);

    app->listener = nfc_listener_alloc(app->nfc, NfcProtocolMfClassic, app->mf_classic_data);
    if(!app->listener) {
        FURI_LOG_E(TAG, "Failed to allocate listener");
        emulation_stats_free((EmulationStats*)app->emulation_stats);
        app->emulation_stats = NULL;
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    nfc_listener_start(app->listener, miband_nfc_magic_emulator_callback, app);

    notification_message(app->notifications, &sequence_blink_start_cyan);

    emulation_timer_callback(app);

    if(app->logger) {
        uint8_t* uid = app->mf_classic_data->block[0].data;
        miband_logger_log(
            app->logger,
            LogLevelInfo,
            "Magic template prepared with UID: %02X %02X %02X %02X",
            uid[0],
            uid[1],
            uid[2],
            uid[3]);
    }

    FURI_LOG_I(TAG, "Started blank template emulation for Mi Band");
}

bool miband_nfc_scene_magic_emulator_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MiBandNfcCustomEventCardDetected: {
            EmulationStats* stats = (EmulationStats*)app->emulation_stats;
            if(stats && stats->mutex) {
                if(furi_mutex_acquire(stats->mutex, 100) == FuriStatusOk) {
                    furi_string_set_str(stats->status_message, "Mi Band actively reading");
                    furi_mutex_release(stats->mutex);
                }
                notification_message(app->notifications, &sequence_single_vibro);
            }
            consumed = true;
            break;
        }
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        app->is_emulating = false;
        if(app->listener) {
            nfc_listener_stop(app->listener);
            nfc_listener_free(app->listener);
            app->listener = NULL;
        }

        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void miband_nfc_scene_magic_emulator_on_exit(void* context) {
    MiBandNfcApp* app = context;

    if(!app) return;

    app->is_emulating = false;

    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }

    if(app->emulation_stats) {
        emulation_stats_free((EmulationStats*)app->emulation_stats);
        app->emulation_stats = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);

    popup_reset(app->popup);

    FURI_LOG_I(TAG, "Magic emulation stopped");
}
