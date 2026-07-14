// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana
//
// MIFARE Classic dictionary attack scene.
// Tests ~20 public default keys against each sector x key type.
// Typical corporate or transit cards fall in 5-30s
// because the factory keys were never rotated.

#include "../chaos_id_app.h"
#include "chaos_id_scene.h"

#include <string.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

// =========================================================================
// Dictionary of known public default keys
// =========================================================================

static const uint8_t default_keys[][MF_CLASSIC_KEY_SIZE] = {
    // NXP / Mifare factory
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}, // MAD-A
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}, // MAD-B
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    // Comuns em sistemas de transporte/acesso
    {0x71, 0x4C, 0x5C, 0x88, 0x6E, 0x97},
    {0x58, 0x7E, 0xE5, 0xF9, 0x35, 0x0F},
    {0xA0, 0x47, 0x8C, 0xC3, 0x90, 0x91},
    {0x53, 0x3C, 0xB6, 0xC7, 0x23, 0xF6},
    {0x8F, 0xD0, 0xA4, 0xF2, 0x56, 0xE9},
    {0xB5, 0xFF, 0x67, 0xCB, 0xA9, 0x51},
    {0xE0, 0x0E, 0xCF, 0xA8, 0xBF, 0x95},
    // Padroes de teste
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},
    {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB},
    {0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F},
    {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5},
    {0x75, 0xCC, 0xB5, 0x9C, 0x9B, 0xED},
};
#define DEFAULT_KEYS_COUNT (sizeof(default_keys) / MF_CLASSIC_KEY_SIZE)

// =========================================================================
// Worker thread - executa o brute force
// =========================================================================

static int32_t attack_thread_run(void* arg) {
    ChaosIdApp* app = arg;
    AttackContext* ctx = &app->attack;

    // 1. Detect the type (1k/4k/Mini) - also validates the card is in the field
    MfClassicError err = mf_classic_poller_sync_detect_type(app->nfc, &ctx->type);
    if(err != MfClassicErrorNone) {
        FURI_LOG_W("ChaosID", "[attack] detect_type falhou: err=%d", (int)err);
        ctx->failed = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventAttackComplete);
        return 0;
    }

    ctx->total_sectors = mf_classic_get_total_sectors_num(ctx->type);
    FURI_LOG_I(
        "ChaosID",
        "[attack] tipo=%d sectors=%u keys_dict=%u",
        (int)ctx->type,
        ctx->total_sectors,
        (unsigned)DEFAULT_KEYS_COUNT);

    // 2. Main loop: sector x key_type x key
    for(uint8_t s = 0; s < ctx->total_sectors && !ctx->stop; s++) {
        ctx->current_sector = s;
        uint8_t sector_trailer = mf_classic_get_sector_trailer_num_by_sector(s);

        for(int kt_i = 0; kt_i < 2 && !ctx->stop; kt_i++) {
            MfClassicKeyType kt = (kt_i == 0) ? MfClassicKeyTypeA : MfClassicKeyTypeB;
            ctx->current_key_type = kt;

            for(size_t k = 0; k < DEFAULT_KEYS_COUNT && !ctx->stop; k++) {
                ctx->current_key_idx = k;

                MfClassicKey key;
                memcpy(key.data, default_keys[k], MF_CLASSIC_KEY_SIZE);

                MfClassicAuthContext auth_ctx;
                err = mf_classic_poller_sync_auth(app->nfc, sector_trailer, &key, kt, &auth_ctx);

                if(err == MfClassicErrorNone) {
                    // Chave bate! Salva e parte pro proximo key_type
                    if(kt == MfClassicKeyTypeA) {
                        ctx->sector_a_found[s] = true;
                        memcpy(&ctx->found_key_a[s], &key, sizeof(key));
                    } else {
                        ctx->sector_b_found[s] = true;
                        memcpy(&ctx->found_key_b[s], &key, sizeof(key));
                    }
                    ctx->keys_found++;
                    if(ctx->sector_a_found[s] && ctx->sector_b_found[s]) {
                        ctx->sectors_fully_cracked++;
                    }
                    FURI_LOG_I(
                        "ChaosID",
                        "[attack] sector=%u keyType=%s key=%02X%02X%02X%02X%02X%02X OK",
                        s,
                        (kt == MfClassicKeyTypeA) ? "A" : "B",
                        key.data[0],
                        key.data[1],
                        key.data[2],
                        key.data[3],
                        key.data[4],
                        key.data[5]);
                    break; // proximo key_type
                }
            }
        }

        // Update UI after each sector finishes
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventAttackProgress);
    }

    ctx->complete = true;
    view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventAttackComplete);
    return 0;
}

// =========================================================================
// Helpers de UI
// =========================================================================

static const char* mfc_type_label(MfClassicType type) {
    switch(type) {
    case MfClassicTypeMini:
        return "Mifare Mini";
    case MfClassicType1k:
        return "Mifare Classic 1K";
    case MfClassicType4k:
        return "Mifare Classic 4K";
    default:
        return "?";
    }
}

static void update_progress_popup(ChaosIdApp* app) {
    AttackContext* ctx = &app->attack;
    char buf[80];
    snprintf(
        buf,
        sizeof(buf),
        "Sector %u/%u\nKeys OK: %u",
        ctx->current_sector + 1,
        ctx->total_sectors,
        ctx->keys_found);
    popup_set_text(app->popup, buf, 64, 35, AlignCenter, AlignTop);
}

static void show_final_result(ChaosIdApp* app) {
    AttackContext* ctx = &app->attack;

    if(ctx->failed) {
        popup_reset(app->popup);
        popup_set_header(app->popup, "Falha", 64, 12, AlignCenter, AlignTop);
        popup_set_text(
            app->popup, "Card not detected.\nReposition and retry.", 64, 32, AlignCenter, AlignTop);
        return;
    }

    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Result");

    FuriString* text = furi_string_alloc();
    furi_string_cat_printf(text, "Type: %s\n", mfc_type_label(ctx->type));
    furi_string_cat_printf(
        text, "Keys OK: %u/%u\n", ctx->keys_found, (unsigned)(ctx->total_sectors * 2));
    furi_string_cat_printf(
        text, "Sectors 100%%: %u/%u\n\n", ctx->sectors_fully_cracked, ctx->total_sectors);

    if(ctx->sectors_fully_cracked == ctx->total_sectors) {
        furi_string_cat_str(
            text,
            "*** FULLY CLONABLE ***\nDefault dictionary\ncracks this card\nin 30 seconds.\n");
    } else if(ctx->sectors_fully_cracked > 0) {
        furi_string_cat_str(text, "PARTIALLY BROKEN\nNested attack can\nfinish the rest.\n");
    } else if(ctx->keys_found > 0) {
        furi_string_cat_str(text, "Some keys recovered.\nNested attack can finish the rest.\n");
    } else {
        furi_string_cat_str(
            text, "No default keys.\nResistant to dictionary.\nNested required.\n");
    }

    // List recovered keys
    if(ctx->keys_found > 0) {
        furi_string_cat_str(text, "\nChaves:\n");
        for(uint8_t s = 0; s < ctx->total_sectors; s++) {
            if(ctx->sector_a_found[s]) {
                furi_string_cat_printf(
                    text,
                    "S%u A: %02X%02X%02X%02X%02X%02X\n",
                    s,
                    ctx->found_key_a[s].data[0],
                    ctx->found_key_a[s].data[1],
                    ctx->found_key_a[s].data[2],
                    ctx->found_key_a[s].data[3],
                    ctx->found_key_a[s].data[4],
                    ctx->found_key_a[s].data[5]);
            }
            if(ctx->sector_b_found[s]) {
                furi_string_cat_printf(
                    text,
                    "S%u B: %02X%02X%02X%02X%02X%02X\n",
                    s,
                    ctx->found_key_b[s].data[0],
                    ctx->found_key_b[s].data[1],
                    ctx->found_key_b[s].data[2],
                    ctx->found_key_b[s].data[3],
                    ctx->found_key_b[s].data[4],
                    ctx->found_key_b[s].data[5]);
            }
        }
    }

    widget_add_text_scroll_element(app->widget, 0, 14, 128, 50, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewWidget);
}

// =========================================================================
// Scene callbacks
// =========================================================================

void chaos_id_scene_attack_on_enter(void* context) {
    ChaosIdApp* app = context;
    AttackContext* ctx = &app->attack;

    // Reset state
    memset(ctx, 0, sizeof(AttackContext));

    // UI inicial
    popup_reset(app->popup);
    popup_set_header(app->popup, "Investigating", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Hold the card\non the Flipper", 64, 35, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewScanning);

    // Spawn worker thread
    ctx->thread = furi_thread_alloc_ex("MFCAttack", 2048, attack_thread_run, app);
    furi_thread_start(ctx->thread);
}

bool chaos_id_scene_attack_on_event(void* context, SceneManagerEvent event) {
    ChaosIdApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ChaosIdCustomEventAttackProgress:
            update_progress_popup(app);
            consumed = true;
            break;
        case ChaosIdCustomEventAttackComplete:
            show_final_result(app);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Sinaliza thread pra parar; on_exit faz join
        app->attack.stop = true;
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ChaosIdSceneSplash);
        consumed = true;
    }

    return consumed;
}

void chaos_id_scene_attack_on_exit(void* context) {
    ChaosIdApp* app = context;
    AttackContext* ctx = &app->attack;

    // Para e joina a thread (espera ate ela sair do loop)
    ctx->stop = true;
    if(ctx->thread) {
        furi_thread_join(ctx->thread);
        furi_thread_free(ctx->thread);
        ctx->thread = NULL;
    }

    popup_reset(app->popup);
    widget_reset(app->widget);
}
