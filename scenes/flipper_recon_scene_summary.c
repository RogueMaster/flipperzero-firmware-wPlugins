#include "../flipper_recon_i.h"

void flipper_recon_scene_summary_on_enter(void* context) {
    FlipperReconApp* app = context;
    Session* session = app->session;
    Widget* widget = app->widget;
    widget_reset(widget);

    /* effective (propagated) risk per asset */
    uint8_t risk[RECON_MAX_ASSETS];
    graph_propagate_risk(session, risk);

    uint32_t sum = 0;
    uint8_t top_eff = 0;
    uint16_t top_idx = RECON_INVALID_INDEX;
    for(uint16_t i = 0; i < session->asset_count; i++) {
        sum += session->assets[i].risk;
        if(risk[i] >= top_eff) {
            top_eff = risk[i];
            top_idx = i;
        }
    }
    uint8_t avg = session->asset_count ? (uint8_t)(sum / session->asset_count) : 0;

    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "%s\nAssets: %u   Relations: %u\nEvidence: %u\nAvg risk: %u\n",
        session->name,
        session->asset_count,
        session->relation_count,
        session->evidence_count,
        avg);
    if(top_idx != RECON_INVALID_INDEX) {
        furi_string_cat_printf(
            text, "Top risk: %s (eff %u)", session->assets[top_idx].name, top_eff);
    } else {
        furi_string_cat_str(text, "Top risk: -");
    }

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool flipper_recon_scene_summary_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipper_recon_scene_summary_on_exit(void* context) {
    FlipperReconApp* app = context;
    widget_reset(app->widget);
}
