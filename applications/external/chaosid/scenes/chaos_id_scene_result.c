// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#include "../chaos_id_app.h"
#include "chaos_id_scene.h"

#include <string.h>

static void result_button_callback(GuiButtonType type, InputType input, void* context) {
    ChaosIdApp* app = context;
    if(input == InputTypeShort && type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ChaosIdCustomEventInvestigate);
    }
}

void chaos_id_scene_result_on_enter(void* context) {
    ChaosIdApp* app = context;
    widget_reset(app->widget);

    const CardProfile* p = app->last_result;
    if(!p) {
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Sem resultado");
        view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewWidget);
        return;
    }

    // Botao "Investigar" so aparece pra MIFARE Classic com risco Broken
    // (cartao alvo do ataque de dicionario v0.5)
    bool can_attack = (p->risk == CardRiskBroken) &&
                      (strstr(p->protocol, "MIFARE Classic") != NULL);

    FuriString* dossier = furi_string_alloc();
    furi_string_cat_printf(dossier, "Freq: %s\n", card_frequency_label(p->frequency));
    furi_string_cat_printf(dossier, "UID: %s\n", app->uid_buffer);
    furi_string_cat_printf(dossier, "Dados: %s\n", app->card_data_buffer);
    furi_string_cat_str(dossier, "\n");
    if(p->typical_use) {
        furi_string_cat_printf(dossier, "Uso: %s\n", p->typical_use);
    }
    furi_string_cat_printf(dossier, "Cripto: %s\n", p->crypto ? p->crypto : "-");
    furi_string_cat_printf(dossier, "Ataque: %s\n", p->attack_vector ? p->attack_vector : "-");
    if(p->year_broken && strcmp(p->year_broken, "-") != 0) {
        furi_string_cat_printf(dossier, "Quebra: %s\n", p->year_broken);
    }
    furi_string_cat_printf(dossier, "Risco: %s\n", card_risk_label(p->risk));
    if(p->notes) {
        furi_string_cat_str(dossier, "\n");
        furi_string_cat_str(dossier, p->notes);
    }

    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, p->protocol);

    // Encolhe area de scroll quando ha botao pra nao sobrepor
    uint8_t scroll_h = can_attack ? 36 : 50;
    widget_add_text_scroll_element(
        app->widget, 0, 14, 128, scroll_h, furi_string_get_cstr(dossier));

    if(can_attack) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Investigar", result_button_callback, app);
    }

    furi_string_free(dossier);
    view_dispatcher_switch_to_view(app->view_dispatcher, ChaosIdViewWidget);
}

bool chaos_id_scene_result_on_event(void* context, SceneManagerEvent event) {
    ChaosIdApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ChaosIdCustomEventInvestigate) {
            scene_manager_next_scene(app->scene_manager, ChaosIdSceneAttack);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ChaosIdSceneSplash);
        consumed = true;
    }

    return consumed;
}

void chaos_id_scene_result_on_exit(void* context) {
    ChaosIdApp* app = context;
    widget_reset(app->widget);
}
