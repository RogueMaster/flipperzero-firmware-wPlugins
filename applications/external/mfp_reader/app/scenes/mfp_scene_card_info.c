#include "../mfp_app.h"
#include "../mfp_card_info.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <furi.h>
#include <string.h>

typedef enum {
    CardInfoEventDump = 0,
} CardInfoCustomEvent;

static void card_info_dump_cb(GuiButtonType type, InputType input, void* ctx) {
    UNUSED(type);
    if(input != InputTypeShort) return;
    MfpApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, CardInfoEventDump);
}

void mfp_scene_card_info_on_enter(void* ctx) {
    MfpApp* app = ctx;

    widget_reset(app->widget);

    FuriString* text = furi_string_alloc();

    /* Pre-dump preview: Read scene just detected the card and there's
     * no scan data yet, nor was the data loaded from a file. */
    bool pre_dump = !app->loaded_from_file && app->scan_sectors_ok == 0;

    /* Prefer sector 0 block 0 if available for BCC checks. */
    const uint8_t* block0 = NULL;
    if(app->read_all_mode && app->sector_results[0].status == MfpSectorOk) {
        block0 = app->blocks[0];
    } else if(app->loaded_from_file) {
        block0 = app->blocks[0];
    }

    mfp_card_info_format(
        text,
        &app->version,
        app->sak,
        app->atqa,
        app->ats_len > 0 ? app->ats_bytes : NULL,
        app->ats_len,
        block0);

    /* Scrollable text area. Leave 14px at the bottom for the Dump
     * button in preview mode; otherwise use full screen height. */
    widget_add_text_scroll_element(
        app->widget, 0, 0, 128, pre_dump ? 50 : 64, furi_string_get_cstr(text));
    if(pre_dump) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Dump", card_info_dump_cb, app);
    }
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewWidget);
}

bool mfp_scene_card_info_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == CardInfoEventDump) {
        app->read_all_mode = true;
        /* Always show the dict picker — it's the single discoverable
         * place where the user can see what dictionaries exist and
         * add their own. */
        scene_manager_next_scene(app->scene_manager, MfpSceneDictSelect);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        /* If Actions is anywhere up the stack we got here through the
         * post-dump menu — let the default back handler pop us back
         * into Actions. */
        if(scene_manager_has_previous_scene(app->scene_manager, MfpSceneActions)) {
            return false;
        }
        /* Otherwise we're in the post-detection preview flow: Read is
         * the immediate previous scene and would restart card detection
         * on its on_enter, so jump straight back to Start. */
        if(scene_manager_has_previous_scene(app->scene_manager, MfpSceneRead)) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MfpSceneStart);
            return true;
        }
        return false;
    }
    return false;
}

void mfp_scene_card_info_on_exit(void* ctx) {
    MfpApp* app = ctx;
    widget_reset(app->widget);
}
