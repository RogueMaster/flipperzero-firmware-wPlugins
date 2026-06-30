#include "lora_relay.h"

#include <furi.h>
#include <furi_hal.h>

// Need access to u8g2
#include <gui/canvas_i.h>
#include <gui/gui_i.h>
#include <u8g2_glue.h>

#include <dialogs/dialogs.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>

#include <storage/storage.h>

#include "view_lora_rx.h"
#include "view_lora_tx.h"

#define LORA_APP_FOLDER "apps_data/lora"

static FuriHalSpiBusHandle* spi = &furi_hal_spi_bus_handle_external;

const GpioPin* const pin_led = &gpio_swclk;
const GpioPin* const pin_back = &gpio_button_back;

#define TAG "LoRaRelay"

void abandone();
void configureRadioEssentials();
bool begin();
bool sanityCheck();
void checkBusy();
void setModeReceive();
int lora_receive_async(uint8_t* buff, int buffMaxLen);
bool configSetFrequency(long frequencyInHz);
bool configSetBandwidth(int bw);
bool configSetSpreadingFactor(int sf);

// Our application menu has 2 items.  You can add more items if you want.
typedef enum {
    LoRaRelaySubmenuIndexConfigure,
    LoRaRelaySubmenuIndexAbout,
} LoRaRelaySubmenuIndex;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    ViewLoRaRX* view_lora_rx;
    ViewLoRaTX* view_lora_tx;
    Widget* widget_about; // The about screen
    DialogsApp* dialogs;

    VariableItemList* variable_item_list;
    Submenu* submenu;

    uint8_t config_bw;
    uint8_t config_frequency;
    uint8_t config_sf;
} LoRaRelay;

typedef enum {
    LoRaRelayViewSubmenu,
    LoRaRelayViewConfigure,
    LoRaRelayViewLoRaRX,
    LoRaRelayViewLoRaTX,
    LoRaRelayViewLoRaAbout,
} LoRaRelayView;

const uint8_t config_bw_value[] = {
    0x00,
    0x08,
    0x01,
    0x09,
    0x02,
    0x0A,
    0x03,
    0x04,
    0x05,
    0x06,
};
const char* const config_bw_text[] = {
    "7.81 kHz",
    "10.42 kHz",
    "15.63 kHz",
    "20.83 kHz",
    "31.25 kHz",
    "41.67 kHz",
    "62.50 kHz",
    "125 kHz",
    "250 kHz",
    "500 kHz",
};

const uint8_t config_sf_value[] = {
    0x05,
    0x06,
    0x07,
    0x08,
    0x09,
    0x0A,
    0x0B,
    0x0C,
};
const char* const config_sf_text[] = {
    "SF5",
    "SF6",
    "SF7",
    "SF8",
    "SF9",
    "SF10",
    "SF11",
    "SF12",
};

// void lora_make_app_folder(LoRaRelay* instance) {
//     furi_assert(instance);

//     if(!storage_simply_mkdir(instance->storage, LORA_APP_FOLDER)) {
//         dialog_message_show_storage_error(instance->dialogs, "Cannot
//         create\napp folder");
//     }
// }

static void lora_relay_submenu_callback(void* context, uint32_t index) {
    LoRaRelay* instance = (LoRaRelay*)context;
    view_dispatcher_switch_to_view(instance->view_dispatcher, index);
    // switch(index) {
    //     case LoRaRelaySubmenuIndexConfigure:
    //         view_dispatcher_switch_to_view(instance->view_dispatcher,
    //         LoRaRelayViewSubmenu); break;
    // case LoRaRelaySubmenuIndexAbout:
    //         view_dispatcher_switch_to_view(instance->view_dispatcher,
    //         LoRaRelayViewLoRaAbout); break;
    // default:
    //     break;
    // }
}

static uint32_t lora_relay_previous_callback(void* context) {
    UNUSED(context);
    return LoRaRelayViewSubmenu;
}

static uint32_t lora_relay_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void lora_relay_reload_config(LoRaRelay* instance) {
    FURI_LOG_I(
        TAG,
        "frequency: %d, sf: %d, bw: %d",
        instance->config_frequency,
        instance->config_sf,
        instance->config_bw);
}

static void lora_config_set_bw(VariableItem* item) {
    LoRaRelay* instance = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_bw_text[index]);
    instance->config_bw = config_bw_value[index];

    configSetBandwidth(config_bw_value[index]);

    lora_relay_reload_config(instance);
}

static void lora_config_set_sf(VariableItem* item) {
    LoRaRelay* instance = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_sf_text[index]);
    instance->config_sf = config_sf_value[index];

    configSetSpreadingFactor(config_sf_value[index]);

    lora_relay_reload_config(instance);
}

static void lora_config_set_frequency(VariableItem* item) {
    LoRaRelay* instance = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    FuriString* temp;
    temp = furi_string_alloc();
    furi_string_cat_printf(temp, "%d", index + 883);
    variable_item_set_current_value_text(item, furi_string_get_cstr(temp));
    furi_string_free(temp);
    instance->config_frequency = index;

    configSetFrequency((index + 883) * 1000000);

    lora_relay_reload_config(instance);
}

LoRaRelay* lora_relay_alloc() {
    LoRaRelay* instance = malloc(sizeof(LoRaRelay));

    View* view = NULL;

    instance->gui = furi_record_open(RECORD_GUI);

    instance->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    // Configure
    instance->variable_item_list = variable_item_list_alloc();
    view = variable_item_list_get_view(instance->variable_item_list);
    view_set_previous_callback(view, lora_relay_previous_callback);
    view_dispatcher_add_view(instance->view_dispatcher, LoRaRelayViewConfigure, view);

    // RX
    instance->view_lora_rx = view_lora_rx_alloc();
    view = view_lora_rx_get_view(instance->view_lora_rx);
    view_set_previous_callback(view, lora_relay_previous_callback);
    view_dispatcher_add_view(instance->view_dispatcher, LoRaRelayViewLoRaRX, view);

    // TX
    instance->view_lora_tx = view_lora_tx_alloc();
    view = view_lora_tx_get_view(instance->view_lora_tx);
    view_set_previous_callback(view, lora_relay_previous_callback);
    view_dispatcher_add_view(instance->view_dispatcher, LoRaRelayViewLoRaTX, view);

    // Configuration items
    VariableItem* item;
    instance->config_bw = 0x04;
    instance->config_frequency = 32;
    instance->config_sf = 0x08;
    // Frequency
    item = variable_item_list_add(
        instance->variable_item_list, "Frequency:", 64, lora_config_set_frequency, instance);
    variable_item_set_current_value_index(item, 32);
    variable_item_set_current_value_text(item, "915");
    // Band Width
    item = variable_item_list_add(
        instance->variable_item_list,
        "Band Width:",
        COUNT_OF(config_bw_value),
        lora_config_set_bw,
        instance);
    variable_item_set_current_value_index(item, 7);
    variable_item_set_current_value_text(item, config_bw_text[7]);
    // Spread Factor
    item = variable_item_list_add(
        instance->variable_item_list,
        "Spread Factor:",
        COUNT_OF(config_sf_value),
        lora_config_set_sf,
        instance);
    variable_item_set_current_value_index(item, 3);
    variable_item_set_current_value_text(item, config_sf_text[3]);

    // Menu
    instance->submenu = submenu_alloc();
    view = submenu_get_view(instance->submenu);
    view_set_previous_callback(view, lora_relay_exit_callback);
    view_dispatcher_add_view(instance->view_dispatcher, LoRaRelayViewSubmenu, view);
    submenu_add_item(
        instance->submenu,
        "LoRa settings",
        LoRaRelayViewConfigure,
        lora_relay_submenu_callback,
        instance);
    submenu_add_item(
        instance->submenu,
        "LoRa sniffer",
        LoRaRelayViewLoRaRX,
        lora_relay_submenu_callback,
        instance);
    submenu_add_item(
        instance->submenu,
        "LoRa sender",
        LoRaRelayViewLoRaTX,
        lora_relay_submenu_callback,
        instance);
    submenu_add_item(
        instance->submenu, "About", LoRaRelayViewLoRaAbout, lora_relay_submenu_callback, instance);

    instance->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        instance->widget_about,
        0,
        0,
        128,
        64,
        "This is a sample application.\n---\nReplace code and message\nwith your "
        "content!\n\nauthor: "
        "@codeallnight\nhttps://discord.com/invite/NsjCvqwPAd\nhttps://"
        "youtube.com/@MrDerekJamison");
    view_set_previous_callback(
        widget_get_view(instance->widget_about), lora_relay_previous_callback);
    view_dispatcher_add_view(
        instance->view_dispatcher,
        LoRaRelayViewLoRaAbout,
        widget_get_view(instance->widget_about));

    return instance;
}

void lora_relay_free(LoRaRelay* instance) {
    view_dispatcher_remove_view(instance->view_dispatcher, LoRaRelayViewSubmenu);
    submenu_free(instance->submenu);

    view_dispatcher_remove_view(instance->view_dispatcher, LoRaRelayViewConfigure);
    variable_item_list_free(instance->variable_item_list);

    view_dispatcher_remove_view(instance->view_dispatcher, LoRaRelayViewLoRaRX);
    view_dispatcher_remove_view(instance->view_dispatcher, LoRaRelayViewLoRaTX);
    view_dispatcher_remove_view(instance->view_dispatcher, LoRaRelayViewLoRaAbout);
    widget_free(instance->widget_about);
    view_lora_rx_free(instance->view_lora_rx);
    view_lora_tx_free(instance->view_lora_tx);

    view_dispatcher_free(instance->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(instance);
}

int32_t lora_relay_run(LoRaRelay* instance) {
    UNUSED(instance);
    view_dispatcher_switch_to_view(instance->view_dispatcher, LoRaRelayViewSubmenu);
    view_dispatcher_run(instance->view_dispatcher);

    return 0;
}

int32_t lora_relay_app(void* p) {
    UNUSED(p);

    spi->cs = &gpio_ext_pc0;

    furi_hal_spi_bus_handle_init(spi);

    abandone();

    begin();

    LoRaRelay* instance = lora_relay_alloc();

    int32_t ret = lora_relay_run(instance);

    lora_relay_free(instance);

    furi_hal_spi_bus_handle_deinit(spi);
    spi->cs = &gpio_ext_pa4;

    // Typically when a pin is no longer in use, it is set to analog mode.
    furi_hal_gpio_init_simple(pin_led, GpioModeAnalog);

    return ret;
}
