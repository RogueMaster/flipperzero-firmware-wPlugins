/*
 * scene_radio — edit the node's radio settings by hand and push them.
 *
 * Profiles is for cloning a known-good setup onto many nodes; this is for the
 * one-off tweak, so it edits the four numbers directly on a VariableItemList
 * (left/right selectors, no keyboard) and reuses the proven apply path to send
 * and verify them.
 *
 * Frequency is stepped in 25 kHz around the node's current value rather than
 * offered as an absolute band: that keeps the list short and works in any
 * region without a table of channel plans. Bandwidth, spreading factor and
 * coding rate are their real discrete sets.
 *
 * The build-and-verify is meshcore_apply's, so the checkmark means the node
 * actually took the change (re-read SELF_INFO), not just that it answered OK.
 */
#include "../config/meshcore_apply.h"
#include "../meshcore_cfg.h"

#define MESHCORE_RADIO_EVENT_DONE 0x440u
#define MESHCORE_RADIO_WORKER_STACK 2048u

/* 25 kHz steps, centred on the current frequency: index 0 is -1 MHz, the
 * middle is the node's value, the top is +1 MHz. 81 steps covers a tweak
 * without a region table. */
#define MESHCORE_RADIO_FREQ_STEP_KHZ 25u
#define MESHCORE_RADIO_FREQ_STEPS 81u
#define MESHCORE_RADIO_FREQ_MID (MESHCORE_RADIO_FREQ_STEPS / 2u)

static const uint32_t MESHCORE_RADIO_BW_HZ[] = {62500u, 125000u, 250000u, 500000u};
#define MESHCORE_RADIO_BW_COUNT (sizeof(MESHCORE_RADIO_BW_HZ) / sizeof(MESHCORE_RADIO_BW_HZ[0]))

#define MESHCORE_RADIO_SF_MIN 5u
#define MESHCORE_RADIO_SF_COUNT 8u /* SF5..SF12 */
#define MESHCORE_RADIO_CR_MIN 5u
#define MESHCORE_RADIO_CR_COUNT 4u /* 4/5..4/8 */
#define MESHCORE_RADIO_TX_MIN 1u /* dBm; index 0 == 1 dBm */
#define MESHCORE_RADIO_TX_MAX 30u /* selector upper bound if the node hides its max */

typedef enum {
    MeshCoreRadioItemFreq,
    MeshCoreRadioItemBw,
    MeshCoreRadioItemSf,
    MeshCoreRadioItemCr,
    MeshCoreRadioItemTx,
    MeshCoreRadioItemApply,
} MeshCoreRadioItem;

/* The edited values live on the app between the change callbacks and the apply
 * worker; a small struct keeps them in one place. Frequency's base is the node
 * value at scene entry, so the index maps back to an absolute kHz. */
static struct {
    uint32_t freq_base_khz;
    uint8_t freq_index;
    uint8_t bw_index;
    uint8_t sf_index;
    uint8_t cr_index;
    uint8_t tx_index; /* dBm = MESHCORE_RADIO_TX_MIN + tx_index */
    uint8_t tx_count; /* selector length, from the node's max_tx_power */
    VariableItem* freq_item;
} radio;

static uint32_t meshcore_radio_freq_khz(void) {
    /* base is the midpoint, so index MID reproduces the node's value exactly. */
    int32_t offset = (int32_t)radio.freq_index - (int32_t)MESHCORE_RADIO_FREQ_MID;
    return radio.freq_base_khz + offset * (int32_t)MESHCORE_RADIO_FREQ_STEP_KHZ;
}

static void meshcore_radio_build_preset(MeshCorePreset* preset) {
    memset(preset, 0, sizeof(*preset));
    snprintf(preset->name, sizeof(preset->name), "manual");
    preset->freq_khz = meshcore_radio_freq_khz();
    preset->bw_hz = MESHCORE_RADIO_BW_HZ[radio.bw_index];
    preset->sf = (uint8_t)(MESHCORE_RADIO_SF_MIN + radio.sf_index);
    preset->cr = (uint8_t)(MESHCORE_RADIO_CR_MIN + radio.cr_index);
    preset->has_tx_power = true;
    preset->tx_power = (uint8_t)(MESHCORE_RADIO_TX_MIN + radio.tx_index);
    /* Radio-only edit: leave path hash at the firmware default and touch no
     * name, so apply sends SET_RADIO_PARAMS (+ SET_TX_POWER) and nothing else. */
    preset->path_hash_bytes = 1;
    preset->has_node_name = false;
}

/* ---- value change callbacks (GUI thread) ---- */

static void meshcore_radio_freq_changed(VariableItem* item) {
    radio.freq_index = variable_item_get_current_value_index(item);
    char text[MESHCORE_PRESET_FIELD_LEN];
    meshcore_preset_format_freq(meshcore_radio_freq_khz(), text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void meshcore_radio_bw_changed(VariableItem* item) {
    radio.bw_index = variable_item_get_current_value_index(item);
    char text[MESHCORE_PRESET_FIELD_LEN];
    meshcore_preset_format_bw(MESHCORE_RADIO_BW_HZ[radio.bw_index], text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void meshcore_radio_sf_changed(VariableItem* item) {
    radio.sf_index = variable_item_get_current_value_index(item);
    char text[8];
    snprintf(text, sizeof(text), "SF%u", (unsigned)(MESHCORE_RADIO_SF_MIN + radio.sf_index));
    variable_item_set_current_value_text(item, text);
}

static void meshcore_radio_cr_changed(VariableItem* item) {
    radio.cr_index = variable_item_get_current_value_index(item);
    char text[8];
    snprintf(text, sizeof(text), "4/%u", (unsigned)(MESHCORE_RADIO_CR_MIN + radio.cr_index));
    variable_item_set_current_value_text(item, text);
}

static void meshcore_radio_tx_changed(VariableItem* item) {
    radio.tx_index = variable_item_get_current_value_index(item);
    char text[8];
    snprintf(text, sizeof(text), "%u dBm", (unsigned)(MESHCORE_RADIO_TX_MIN + radio.tx_index));
    variable_item_set_current_value_text(item, text);
}

/* Nearest index for a value in a min+index set, clamped to the list. */
static uint8_t meshcore_radio_nearest(uint32_t value, uint32_t min, uint8_t count) {
    if(value <= min) return 0;
    uint8_t idx = (uint8_t)(value - min);
    return (idx >= count) ? (uint8_t)(count - 1) : idx;
}

static uint8_t meshcore_radio_bw_nearest(uint32_t bw_hz) {
    uint8_t best = 0;
    uint32_t best_d = UINT32_MAX;
    for(uint8_t i = 0; i < MESHCORE_RADIO_BW_COUNT; i++) {
        uint32_t d = (MESHCORE_RADIO_BW_HZ[i] > bw_hz) ? MESHCORE_RADIO_BW_HZ[i] - bw_hz :
                                                         bw_hz - MESHCORE_RADIO_BW_HZ[i];
        if(d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

/* ---- apply worker ---- */

/* Send one applicable step and wait for OK. Returns NULL, or a literal error.
 * A step that does not apply is a no-op success. */
static const char* meshcore_radio_send_step(
    MeshCoreApp* app,
    const MeshCorePreset* preset,
    MeshCoreApplyStep step,
    const char* refused) {
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;
    size_t len = meshcore_apply_build(preset, step, payload, sizeof(payload));
    if(len == 0) return NULL; /* nothing to send for this step */
    if(!meshcore_session_request(
           app->session, payload, len, MC_RESP_OK, &event, MESHCORE_LINK_TIMEOUT_MS)) {
        return (event.code == MC_RESP_ERR) ? refused : "No answer from the node.";
    }
    return NULL;
}

static int32_t meshcore_radio_worker(void* context) {
    MeshCoreApp* app = context;

    MeshCorePreset preset;
    meshcore_radio_build_preset(&preset);

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;

    /* Connect ourselves if needed, like the messenger — the user should not
     * have to visit Connect before applying. Idempotent when already up. */
    app->worker_error = meshcore_connect_ensure(app);

    /* Radio params, then TX power — two commands, one confirming read-back. */
    if(app->worker_error == NULL) {
        app->worker_error =
            meshcore_radio_send_step(app, &preset, MeshCoreApplyRadio, "Node refused the settings.");
    }
    if(app->worker_error == NULL) {
        app->worker_error =
            meshcore_radio_send_step(app, &preset, MeshCoreApplyTxPower, "Node refused TX power.");
    }

    if(app->worker_error == NULL) {
        /* Answered OK — now prove it took by re-reading, the same discipline as
         * the Apply screen. One SELF_INFO confirms both radio and TX power. */
        size_t len = mc_cmd_app_start(payload, sizeof(payload), MESHCORE_LINK_APP_NAME);
        if(len == 0 || !meshcore_session_request(
                           app->session,
                           payload,
                           len,
                           MC_RESP_SELF_INFO,
                           &event,
                           MESHCORE_LINK_TIMEOUT_MS)) {
            app->worker_error = "Set, but could not confirm.";
        } else if(!meshcore_apply_verify_radio(&preset, &event.u.self_info)) {
            app->worker_error = "Node said OK but did not change.";
        } else if(!meshcore_apply_verify_tx(&preset, &event.u.self_info)) {
            app->worker_error = "TX power did not change.";
        } else {
            /* Confirmed: keep the app's view of the node in step so other
             * screens show the new numbers. */
            app->node.freq_khz = preset.freq_khz;
            app->node.bw_hz = preset.bw_hz;
            app->node.sf = preset.sf;
            app->node.cr = preset.cr;
            app->node.tx_power = preset.tx_power;
            meshcore_log_printf(
                app->log,
                "radio set: %lu kHz bw %lu sf%u cr%u tx%u",
                (unsigned long)preset.freq_khz,
                (unsigned long)preset.bw_hz,
                (unsigned)preset.sf,
                (unsigned)preset.cr,
                (unsigned)preset.tx_power);
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_RADIO_EVENT_DONE);
    return 0;
}

/* Enter (OK) on the Apply row starts the send. The worker connects itself if
 * needed and reports a failure to reach the node, so there is no connection
 * pre-check here. */
static void meshcore_radio_enter(void* context, uint32_t index) {
    MeshCoreApp* app = context;
    if(index != MeshCoreRadioItemApply) return;
    if(app->worker != NULL) return; /* one apply in flight */

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "\e#Applying...");
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreRadio", MESHCORE_RADIO_WORKER_STACK, meshcore_radio_worker, app);
    furi_thread_start(app->worker);
}

/* ---- scene ---- */

static void meshcore_radio_populate(MeshCoreApp* app) {
    VariableItemList* list = app->var_list;
    VariableItem* item;
    char text[MESHCORE_PRESET_FIELD_LEN];

    variable_item_list_reset(list);

    /* Seed the selections from the node's current radio, so opening the editor
     * shows what the node is actually doing. */
    radio.freq_base_khz = app->node.freq_khz ? app->node.freq_khz : 869525u;
    radio.freq_index = MESHCORE_RADIO_FREQ_MID;
    radio.bw_index = meshcore_radio_bw_nearest(app->node.bw_hz ? app->node.bw_hz : 250000u);
    radio.sf_index = meshcore_radio_nearest(app->node.sf ? app->node.sf : 11u, MESHCORE_RADIO_SF_MIN, MESHCORE_RADIO_SF_COUNT);
    radio.cr_index = meshcore_radio_nearest(app->node.cr ? app->node.cr : 5u, MESHCORE_RADIO_CR_MIN, MESHCORE_RADIO_CR_COUNT);
    /* Offer 1 dBm..the node's max (falling back to 22 if it did not say). */
    uint8_t tx_max = app->node.max_tx_power ? app->node.max_tx_power : 22u;
    if(tx_max > MESHCORE_RADIO_TX_MAX) tx_max = MESHCORE_RADIO_TX_MAX;
    if(tx_max < MESHCORE_RADIO_TX_MIN) tx_max = MESHCORE_RADIO_TX_MIN;
    radio.tx_count = tx_max; /* values 1..tx_max */
    radio.tx_index =
        meshcore_radio_nearest(app->node.tx_power ? app->node.tx_power : 22u, MESHCORE_RADIO_TX_MIN, radio.tx_count);

    item = variable_item_list_add(
        list, "Freq", MESHCORE_RADIO_FREQ_STEPS, meshcore_radio_freq_changed, app);
    variable_item_set_current_value_index(item, radio.freq_index);
    meshcore_preset_format_freq(meshcore_radio_freq_khz(), text, sizeof(text));
    variable_item_set_current_value_text(item, text);
    radio.freq_item = item;

    item = variable_item_list_add(
        list, "Band", MESHCORE_RADIO_BW_COUNT, meshcore_radio_bw_changed, app);
    variable_item_set_current_value_index(item, radio.bw_index);
    meshcore_preset_format_bw(MESHCORE_RADIO_BW_HZ[radio.bw_index], text, sizeof(text));
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(
        list, "SF", MESHCORE_RADIO_SF_COUNT, meshcore_radio_sf_changed, app);
    variable_item_set_current_value_index(item, radio.sf_index);
    snprintf(text, sizeof(text), "SF%u", (unsigned)(MESHCORE_RADIO_SF_MIN + radio.sf_index));
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(
        list, "CR", MESHCORE_RADIO_CR_COUNT, meshcore_radio_cr_changed, app);
    variable_item_set_current_value_index(item, radio.cr_index);
    snprintf(text, sizeof(text), "4/%u", (unsigned)(MESHCORE_RADIO_CR_MIN + radio.cr_index));
    variable_item_set_current_value_text(item, text);

    item = variable_item_list_add(list, "TX", radio.tx_count, meshcore_radio_tx_changed, app);
    variable_item_set_current_value_index(item, radio.tx_index);
    snprintf(text, sizeof(text), "%u dBm", (unsigned)(MESHCORE_RADIO_TX_MIN + radio.tx_index));
    variable_item_set_current_value_text(item, text);

    /* One value, used as a button: OK on it applies. */
    variable_item_list_add(list, "Apply to node", 1, NULL, app);

    variable_item_list_set_enter_callback(list, meshcore_radio_enter, app);
}

void meshcore_scene_radio_on_enter(void* context) {
    MeshCoreApp* app = context;
    meshcore_radio_populate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewVarList);
}

bool meshcore_scene_radio_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == MESHCORE_RADIO_EVENT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }

        char text[128];
        if(app->worker_error == NULL) {
            MeshCorePreset preset;
            meshcore_radio_build_preset(&preset);
            char freq[MESHCORE_PRESET_FIELD_LEN];
            char bw[MESHCORE_PRESET_FIELD_LEN];
            meshcore_preset_format_freq(preset.freq_khz, freq, sizeof(freq));
            meshcore_preset_format_bw(preset.bw_hz, bw, sizeof(bw));
            snprintf(
                text,
                sizeof(text),
                "\e#Set and confirmed\n%s  %s\nSF%u  CR4/%u  %u dBm\n\nBack to edit more.",
                freq,
                bw,
                (unsigned)preset.sf,
                (unsigned)preset.cr,
                (unsigned)preset.tx_power);
        } else {
            snprintf(text, sizeof(text), "\e#Not set\n%s\n\nBack to try again.", app->worker_error);
        }
        widget_reset(app->widget);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
        view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        return true;
    }

    return false;
}

void meshcore_scene_radio_on_exit(void* context) {
    MeshCoreApp* app = context;
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    variable_item_list_reset(app->var_list);
}
