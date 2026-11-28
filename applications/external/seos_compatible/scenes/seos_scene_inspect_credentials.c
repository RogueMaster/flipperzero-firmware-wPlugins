#include "../seos_i.h"
#include "seos_scene.h"

/* Second page of a finished inspection: which credential types the reader
 * accepts, one row each. Nothing here can be changed, so every row has a
 * single value and the list only scrolls. */
static void seos_scene_inspect_credentials_add(Seos* seos, const char* name, bool enabled) {
    VariableItem* item = variable_item_list_add(seos->variable_item_list, name, 1, NULL, NULL);
    variable_item_set_current_value_text(item, enabled ? "On" : "Off");
}

void seos_scene_inspect_credentials_on_enter(void* context) {
    Seos* seos = context;
    const SeosInspectData* d = &seos->seos_emulator->inspect;

    seos_scene_inspect_credentials_add(seos, "Seos", d->seos_enabled);
    for(size_t i = 0; i < SEOS_INSPECT_CRED_COUNT; i++) {
        seos_scene_inspect_credentials_add(seos, seos_inspect_cred_name(i), d->cred_enabled[i]);
    }

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewVariableItemList);
}

bool seos_scene_inspect_credentials_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    /* Back is the only way out and the scene manager already handles it. */
    return false;
}

void seos_scene_inspect_credentials_on_exit(void* context) {
    Seos* seos = context;

    variable_item_list_reset(seos->variable_item_list);
}
