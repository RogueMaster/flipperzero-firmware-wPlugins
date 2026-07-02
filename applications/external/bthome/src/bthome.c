#include <furi.h>
#include <furi_hal_bt.h>
#include <furi_hal_power.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>

#include <bthome_icons.h>

// BTHome UUID: 0xFCD2 (little-endian).
#define BTHOME_UUID_LSB 0xD2
#define BTHOME_UUID_MSB 0xFC

// BTHome AD types.
#define BTHOME_AD_TYPE_FLAGS               0x01
#define BTHOME_AD_TYPE_COMPLETE_LOCAL_NAME 0x09
#define BTHOME_AD_TYPE_SERVICE_DATA        0x16

// BTHome object IDs.
#define BTHOME_OBJ_PACKET_ID 0x00
#define BTHOME_OBJ_BATTERY   0x01
#define BTHOME_OBJ_BUTTON    0x3A

// Button event values.
#define BTHOME_BUTTON_EVENT_NONE       0x00
#define BTHOME_BUTTON_EVENT_PRESS      0x01
#define BTHOME_BUTTON_EVENT_LONG_PRESS 0x04

// App values.
#define APP_LOG_TAG "BTHOME"

#define APP_VIEW_MAIN    0
#define APP_BEACON_TIMER 1000

// Payload offsets.
static size_t payload_battery_value_off = 0;
static size_t payload_button_value_off = 0;
static size_t payload_packet_id_value_off = 0;

typedef struct {
    ViewDispatcher* view_dispatcher;

    View* main_view;

    uint8_t packet_id;

    uint8_t beacon_payload[EXTRA_BEACON_MAX_DATA_SIZE];
    size_t beacon_payload_len;

    FuriTimer* beacon_timer;
} BtHomeApp;

typedef struct {
    bool ok_pressed;
} BtHomeModel;

static void bthome_draw_callback(Canvas* canvas, void* ctx) {
    BtHomeModel* model = ctx;
    furi_assert(model);

    canvas_clear(canvas);

    canvas_set_bitmap_mode(canvas, true);

    canvas_draw_icon(canvas, 3, 6, &I_bthome_123x52);

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str_aligned(canvas, 54, 54, AlignCenter, AlignTop, "Press OK to send");

    if(model->ok_pressed) {
        canvas_draw_icon(canvas, 92, 53, &I_Ok_btn_pressed_13x12);
    } else {
        canvas_draw_icon(canvas, 92, 53, &I_Ok_btn_9x9);
    }
}

static bool bthome_input_callback(InputEvent* event, void* ctx) {
    BtHomeApp* app = ctx;

    switch(event->key) {
    case InputKeyBack: {
        if(event->type == InputTypeShort) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }

        break;
    }

    case InputKeyOk: {
        if((event->type == InputTypeShort) || (event->type == InputTypeLong)) {
            app->beacon_payload[payload_packet_id_value_off] = app->packet_id++;

            app->beacon_payload[payload_battery_value_off] = furi_hal_power_get_pct();

            app->beacon_payload[payload_button_value_off] = event->type == InputTypeShort ?
                                                                BTHOME_BUTTON_EVENT_PRESS :
                                                                BTHOME_BUTTON_EVENT_LONG_PRESS;

            furi_hal_bt_extra_beacon_set_data(app->beacon_payload, app->beacon_payload_len);

            if(!furi_hal_bt_extra_beacon_is_active()) {
                furi_hal_bt_extra_beacon_start();

                furi_timer_start(app->beacon_timer, APP_BEACON_TIMER);
            }
        }

        if((event->type == InputTypePress) || (event->type == InputTypeRelease)) {
            with_view_model(
                app->main_view,
                BtHomeModel * model,
                {
                    if(event->type == InputTypePress) {
                        model->ok_pressed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->ok_pressed = false;
                    }
                },
                true);
        }

        return true;
    }

    default:
        break;
    }

    return false;
}

static void bthome_beacon_timer_callback(void* ctx) {
    BtHomeApp* app = ctx;

    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_hal_bt_extra_beacon_stop();
    }

    app->beacon_payload[payload_button_value_off] = BTHOME_BUTTON_EVENT_NONE;
}

static void bthome_init_beacon_payload(uint8_t* payload, size_t* payload_len) {
    size_t i = 0;

    // Flags data.
    payload[i++] = 2; // Flags length.
    payload[i++] = BTHOME_AD_TYPE_FLAGS;
    payload[i++] = 0x06; // LE General Discoverable Mode + BR/EDR Not Supported.

    // Service data.
    payload[i++] = 10; // Service data length.
    payload[i++] = BTHOME_AD_TYPE_SERVICE_DATA;
    payload[i++] = BTHOME_UUID_LSB;
    payload[i++] = BTHOME_UUID_MSB;
    payload[i++] = 0x44; // no encryption, trigger=true, version=2.

    // Packet ID object data.
    payload[i++] = BTHOME_OBJ_PACKET_ID;
    payload_packet_id_value_off = i;
    payload[i++] = 0x00;

    // Battery object data.
    payload[i++] = BTHOME_OBJ_BATTERY;
    payload_battery_value_off = i;
    payload[i++] = 0x00;

    // Button event object data.
    payload[i++] = BTHOME_OBJ_BUTTON;
    payload_button_value_off = i;
    payload[i++] = BTHOME_BUTTON_EVENT_NONE;

    // Complete local name.
    const char* name = furi_hal_version_get_device_name_ptr();
    size_t name_len = strlen(name);

    // Truncate name if not enough space left.
    //
    // 2 bytes for thead header + the actual name length.
    if(i + 2 + name_len > EXTRA_BEACON_MAX_DATA_SIZE) {
        name_len = EXTRA_BEACON_MAX_DATA_SIZE - (i + 2);

        FURI_LOG_E(APP_LOG_TAG, "name len %zu", name_len);
    }

    payload[i++] = 1 + name_len; // Name object length.
    payload[i++] = BTHOME_AD_TYPE_COMPLETE_LOCAL_NAME;

    memcpy(&payload[i], name, name_len);
    i += name_len;

    *payload_len = i;
}

static BtHomeApp* bthome_app_alloc(void) {
    BtHomeApp* app = malloc(sizeof(BtHomeApp));
    if(!app) {
        FURI_LOG_E(APP_LOG_TAG, "Failed to allocate app");

        return NULL;
    }

    app->packet_id = 0;

    bthome_init_beacon_payload(app->beacon_payload, &app->beacon_payload_len);

    app->beacon_timer = furi_timer_alloc(bthome_beacon_timer_callback, FuriTimerTypeOnce, app);

    furi_hal_bt_extra_beacon_stop();

    GapExtraBeaconConfig config = {
        .min_adv_interval_ms = 100,
        .max_adv_interval_ms = 200,
        .adv_channel_map = GapAdvChannelMapAll,
        .adv_power_level = GapAdvPowerLevel_6dBm,
        .address_type = GapAddressTypePublic,
    };

    memcpy(config.address, furi_hal_version_get_ble_mac(), sizeof(config.address));

    if(!furi_hal_bt_extra_beacon_set_config(&config)) {
        FURI_LOG_E("BTHOME", "Failed to set beacon config");
        free(app);
        return NULL;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E(APP_LOG_TAG, "Failed to allocate view dispatcher");

        free(app);
        return NULL;
    }

    app->main_view = view_alloc();

    view_allocate_model(app->main_view, ViewModelTypeLocking, sizeof(BtHomeModel));

    view_set_draw_callback(app->main_view, bthome_draw_callback);
    view_set_input_callback(app->main_view, bthome_input_callback);
    view_set_context(app->main_view, app);

    view_dispatcher_add_view(app->view_dispatcher, APP_VIEW_MAIN, app->main_view);

    return app;
}

static void bthome_app_free(BtHomeApp* app) {
    furi_hal_bt_extra_beacon_stop();

    furi_timer_stop(app->beacon_timer);
    furi_timer_free(app->beacon_timer);

    view_dispatcher_remove_view(app->view_dispatcher, APP_VIEW_MAIN);
    view_free_model(app->main_view);
    view_free(app->main_view);
    view_dispatcher_free(app->view_dispatcher);

    free(app);
}

int32_t bthome_app(void* p) {
    UNUSED(p);

    BtHomeApp* app = bthome_app_alloc();
    if(!app) {
        return -1;
    }

    Gui* gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, APP_VIEW_MAIN);
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    bthome_app_free(app);

    return 0;
}
