#include <furi.h>
#include <furi_hal.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <locale/locale.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include "lora_app_icons.h"

#define PATHAPP                 "apps_data/lora"
#define PATHAPPEXT              EXT_PATH(PATHAPP)
#define PATHLORA                PATHAPPEXT "/data_%d.log"
#define LORA_LOG_FILE_EXTENSION ".log"

#define MAX_LINE_LENGTH 256

#define TIME_LEN 12
#define DATE_LEN 14

#define CLOCK_TIME_FORMAT     "%.2d:%.2d:%.2d"
#define CLOCK_ISO_DATE_FORMAT "%.4d-%.2d-%.2d"

//static const FuriHalSpiBusHandle* spi = &furi_hal_spi_bus_handle_external;

const GpioPin* const pin_led = &gpio_swclk;
const GpioPin* const pin_back = &gpio_button_back;

#define TAG "LoRa"

uint8_t receiveBuff[255];
char asciiBuff[512];

void abandone();
int16_t getRSSI();
void configureRadioEssentials();
bool begin();
bool sanityCheck();
void checkBusy();
void setModeReceive();
int lora_receive_async(uint8_t* buff, int buffMaxLen);
bool configSetFrequency(long frequencyInHz);
bool configSetBandwidth(int bw);
bool configSetSpreadingFactor(int sf);
bool configSetCodingRate(int cr);
bool configSetSyncWord(uint16_t sw);
void setPacketParams(
    uint16_t packetParam1,
    uint8_t packetParam2,
    uint8_t packetParam3,
    uint8_t packetParam4,
    uint8_t packetParam5);

void transmit(uint8_t* data, int dataLen);

// Change this to BACKLIGHT_AUTO if you don't want the backlight to be continuously on.
#define BACKLIGHT_ON 1

// Our application menu has 6 items.  You can add more items if you want.
typedef enum {
    LoRaSubmenuIndexConfigure,
    LoRaSubmenuIndexLoRaWAN,
    LoRaSubmenuIndexSniffer,
    LoRaSubmenuIndexTransmitter,
    LoRaSubmenuIndexManualTX,
    LoRaSubmenuIndexAbout,
} LoRaSubmenuIndex;

// Each view is a screen we show the user.
typedef enum {
    LoRaViewSubmenu, // The menu when the app starts
    LoRaViewFrequencyInput, // Input for configuring frequency settings
    LoRaViewByteInput, // Input for send data (bytes)
    LoRaViewConfigure, // The configuration screen
    LoRaViewLoRaWAN, // The presets LoRaWAN screen
    LoRaViewSniffer, // Sniffer
    LoraViewTransmitter, // Transmitter
    LoRaViewAbout, // The about screen with directions, link to social channel, etc.
} LoRaView;

typedef enum {
    LoRaEventIdRedrawScreen = 0, // Custom event to redraw the screen
    LoRaEventIdOkPressed = 42, // Custom event to process OK button getting pressed down
} LoRaEventId;

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    NotificationApp* notifications; // Used for controlling the backlight
    Submenu* submenu; // The application menu
    TextInput* frequency_input; // The text input screen
    ByteInput* byte_input; // The byte input screen

    VariableItemList* variable_item_list_config; // The configuration screen
    VariableItemList* variable_item_list_lorawan; // The lorawan presets screen

    View* view_sniffer; // The sniffer screen
    View* view_transmitter; // The transmitter screen
    Widget* widget_about; // The about screen

    VariableItem* config_freq_item; // The frequency setting item (so we can update the frequency)
    char* temp_buffer; // Temporary buffer for text input
    uint32_t temp_buffer_size; // Size of temporary buffer

    uint8_t* byte_buffer; // Temporary buffer for text input
    uint32_t byte_buffer_size; // Size of temporary buffer

    FuriTimer* timer_rx; // Timer for redrawing the sniffer screen
    FuriTimer* timer_tx; // Timer for redrawing the transmitter screen

    uint32_t config_frequency;

    // Order is preamble, header type, packet length, CRC, IQ
    uint16_t packetPreamble;
    uint8_t packetHeaderType;
    uint8_t packetPayloadLength;
    uint8_t packetCRC;
    uint8_t packetInvertIQ;

} LoRaApp;

typedef struct {
    FuriString* config_freq_name; // The frequency setting
    uint32_t config_bw_index; // Bandwidth setting index
    uint32_t config_sf_index; // Spread Factor setting index
    uint32_t config_cr_index; // Coding Rate setting index
    uint32_t config_sw_index; // Coding Rate setting index

    uint32_t config_header_type_index; // Header Type setting index
    uint32_t config_crc_index; // CRC setting index
    uint32_t config_iq_index; // IQ setting index

    uint32_t config_region_index; // Frequency plan setting index
    uint32_t config_bw_region_index; // BW region setting index
    uint32_t config_us_dr_index; // US915 Data Rate setting index
    uint32_t config_eu_dr_index; // EU868 Data Rate setting index

    uint32_t config_us915_ul_channels_125k_index;
    uint32_t config_us915_ul_channels_500k_index;
    uint32_t config_eu868_ul_channels_125k_index;

    uint32_t config_eu868_ul_channels_250k_index;
    uint32_t config_us915_dl_channels_500k_index;
    uint32_t config_eu868_dl_channels_rx1_index;

    uint8_t x; // The x coordinate (dummy variable)

    bool flag_file;
    DialogsApp* dialogs_rx;
    Storage* storage_rx;
    File* file_rx;
} LoRaSnifferModel;

typedef struct {
    uint32_t test;
    bool flag_tx_file;
    bool flag_signal;
    FuriString* text;
    DialogsApp* dialogs_tx;
    Storage* storage_tx;
    File* file_tx;
    uint8_t x; // The x coordinate
} LoRaTransmitterModel;

void makePaths(void* context) {
    LoRaApp* app = (LoRaApp*)context;
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    furi_assert(app);
    if(!storage_simply_mkdir(model->storage_rx, PATHAPPEXT)) {
        dialog_message_show_storage_error(model->dialogs_rx, "Cannot create\napp folder");
    }
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t lora_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the submenu.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t lora_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return LoRaViewSubmenu;
}

/**
 * @brief      Callback for returning to configure screen.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the configure screen.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t lora_navigation_configure_callback(void* _context) {
    UNUSED(_context);
    return LoRaViewConfigure;
}

/**
 * @brief      Callback for returning to LoRaWAN screen.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the LoRaWAN screen.
 * @param      _context  The context - unused
 * @return     next view id
*/

// +++++++++++++++ TODO +++++++++++++++
// not used at the moment

// static uint32_t lora_navigation_lorawan_callback(void* _context) {
//     UNUSED(_context);
//     return LoRaViewLoRaWAN;
// }

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - LoRaApp object.
 * @param      index     The LoRaSubmenuIndex item that was clicked.
*/
static void lora_submenu_callback(void* context, uint32_t index) {
    LoRaApp* app = (LoRaApp*)context;
    switch(index) {
    case LoRaSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewConfigure);
        break;
    case LoRaSubmenuIndexLoRaWAN:
        view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewLoRaWAN);
        break;
    case LoRaSubmenuIndexSniffer:
        view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewSniffer);
        break;
    case LoRaSubmenuIndexTransmitter:
        view_dispatcher_switch_to_view(app->view_dispatcher, LoraViewTransmitter);
        break;
    case LoRaSubmenuIndexManualTX:
        view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewByteInput);
        break;
    case LoRaSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewAbout);
        break;
    default:
        break;
    }
}

// Bandwidth configuration
const uint8_t config_bw_values[] = {
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
const char* const config_bw_names[] = {
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

// Spreading Factor configuration
const uint8_t config_sf_values[] = {
    0x05,
    0x06,
    0x07,
    0x08,
    0x09,
    0x0A,
    0x0B,
    0x0C,
};
const char* const config_sf_names[] = {
    "SF5",
    "SF6",
    "SF7",
    "SF8",
    "SF9",
    "SF10",
    "SF11",
    "SF12",
};

// Coding Rate configuration
const uint8_t config_cr_values[] = {
    0x01, // 4/5
    0x02, // 4/6
    0x03, // 4/7
    0x04, // 4/8
};

const char* const config_cr_names[] = {
    "4/5",
    "4/6",
    "4/7",
    "4/8",
};

// Sync Word configuration
const uint16_t config_sw_values[] = {
    0x1424, // Private
    0x3444, // Public (LoRaWAN/TTN)
};

const char* const config_sw_names[] = {
    "Private (0x1424)",
    "Public (0x3444)",
};

const uint8_t config_region_values[] = {
    0x01,
    0x02
    //,
    // 0x03,
    // 0x04,
    // 0x05,
    // 0x06,
    // 0x07,
    // 0x08,
    // 0x09,
    // 0x0A,
    // 0x0B,
    // 0x0C,
    // 0x0D
};

// Regional names
const char* const config_region_names[] = {
    "EU868",
    "US915"
    //,
    // "CN779",
    // "EU433",
    // "AU915",
    // "CN470",
    // "AS923",
    // "AS923-2",
    // "AS923-3",
    // "KR920",
    // "IN865",
    // "RU864",
    // "AS923-4"
};

// Data Rate configuration for US915
const uint8_t config_us_dr_values[] = {
    0x00, // DR0
    0x01, // DR1
    0x02, // DR2
    0x03, // DR3
    0x04, // DR4
    0x08, // DR8
    0x09, // DR9
    0x0A, // DR10
    0x0B, // DR11
    0x0C, // DR12
    0x0D // DR13
};
const char* const config_us_dr_names[] = {
    "SF10/125kHz",
    "SF9/125kHz",
    "SF8/125kHz",
    "SF7/125kHz",
    "SF8/500kHz",
    "SF12/500kHz",
    "SF11/500kHz",
    "SF10/500kHz",
    "SF9/500kHz",
    "SF8/500kHz",
    "SF7/500kHz"};

// Data Rate configuration for EU868
const uint8_t config_eu_dr_values[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

const char* const config_eu_dr_names[] = {
    "SF12/125kHz",
    "SF11/125kHz",
    "SF10/125kHz",
    "SF9/125kHz",
    "SF8/125kHz",
    "SF7/125kHz",
    "SF7/250kHz"};

// Transmit Power configuration for US915
const uint8_t config_txpower_values[] = {
    0x00, // 30 dBm
    0x01, // 28 dBm
    0x02, // 26 dBm
    0x03, // 24 dBm
    0x04, // 22 dBm
    0x05, // 20 dBm
    0x06, // 18 dBm
    0x07, // 16 dBm
    0x08, // 14 dBm
    0x09, // 12 dBm
    0x0A // 10 dBm
};
const char* const config_txpower_names[] = {
    "30 dBm",
    "28 dBm",
    "26 dBm",
    "24 dBm",
    "22 dBm",
    "20 dBm",
    "18 dBm",
    "16 dBm",
    "14 dBm",
    "12 dBm",
    "10 dBm"};

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Uplink channel frequencies for US915 (125 kHz channels)
const uint32_t config_us915_ul_channels_125k[] = {
    902300000, 902500000, 902700000, 902900000, 903100000, 903300000, 903500000, 903700000,
    903900000, 904100000, 904300000, 904500000, 904700000, 904900000, 905100000, 905300000,
    905500000, 905700000, 905900000, 906100000, 906300000, 906500000, 906700000, 906900000,
    907100000, 907300000, 907500000, 907700000, 907900000, 908100000, 908300000, 908500000,
    908700000, 908900000, 909100000, 909300000, 909500000, 909700000, 909900000, 910100000,
    910300000, 910500000, 910700000, 910900000, 911100000, 911300000, 911500000, 911700000,
    911900000, 912100000, 912300000, 912500000, 912700000, 912900000, 913100000, 913300000,
    913500000, 913700000, 913900000, 914100000, 914300000, 914500000, 914700000, 914900000};

// Uplink channel frequencies for US915 (500 kHz channels)
const uint32_t config_us915_ul_channels_500k[] =
    {903000000, 904600000, 906200000, 907800000, 909400000, 911000000, 912600000, 914200000};

// Downlink channel frequencies for US915
const uint32_t config_us915_dl_channels_500k[] =
    {923300000, 923900000, 924500000, 925100000, 925700000, 926300000, 926900000, 927500000};

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Uplink channel frequencies for EU868 (125 kHz default channels)
const uint32_t config_eu868_ul_channels_125k[] = {868100000, 868300000, 868500000};

// Uplink channel frequencies for EU868 (250 kHz channel)
const uint32_t config_eu868_ul_channels_250k[] = {868300000};

// Additional uplink channel frequencies for EU868 (may be used depending on local regulations)
const uint32_t config_eu868_ul_channels_additional[] =
    {867100000, 867300000, 867500000, 867700000, 867900000};

// Downlink channel frequencies for EU868 (RX1 - same as uplink)
const uint32_t config_eu868_dl_channels_rx1[] = {868100000, 868300000, 868500000};

// Downlink channel frequency for EU868 (RX2 - fixed frequency)
const uint32_t config_eu868_dl_channel_rx2 = 869525000;

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Uplink channel frequencies for AS923 (125 kHz default channels)
const uint32_t config_as923_ul_channels_125k[] = {923200000, 923400000};

// Additional uplink channel frequencies for AS923 (may be used depending on local regulations)
const uint32_t config_as923_ul_channels_additional[] =
    {923600000, 923800000, 924000000, 924200000, 924400000, 924600000};

// Downlink channel frequencies for AS923 (RX1 - same as uplink)
const uint32_t config_as923_dl_channels_rx1[] = {923200000, 923400000};

// Downlink channel frequency for AS923 (RX2 - fixed frequency)
const uint32_t config_as923_dl_channel_rx2 = 923200000;

// Frequency offsets for different AS923 sub-bands
const int32_t config_as923_frequency_offsets[] = {
    0, // AS923-1
    -1800000, // AS923-2
    -6600000, // AS923-3
    -5900000 // AS923-4
};

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//Header Type. 0x00 = Variable Len, 0x01 = Fixed Length
const uint8_t config_header_type_values[] = {
    0x00,
    0x01,
};
const char* const config_header_type_names[] = {
    "Variable Len",
    "Fixed Length",
};

//CRC. 0x00 = Off, 0x01 = On
const uint8_t config_crc_values[] = {
    0x00,
    0x01,
};
const char* const config_crc_names[] = {
    "Off",
    "On",
};

//IQ. 0x00 = Standard, 0x01 = Inverted
const uint8_t config_iq_values[] = {
    0x00,
    0x01,
};
const char* const config_iq_names[] = {
    "Standard",
    "Inverted",
};

static const char* config_bw_label = "Bandwidth";

static void lora_config_bw_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_bw_names[index]);
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_bw_index = index;

    configSetBandwidth(config_bw_values[index]);
}

static const char* config_sf_label = "Spread Factor";

static void lora_config_sf_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_sf_names[index]);
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_sf_index = index;

    configSetSpreadingFactor(config_sf_values[index]);
}

static const char* config_cr_label = "Coding Rate";

static void lora_config_cr_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_cr_names[index]);
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_cr_index = index;

    configSetCodingRate(config_cr_values[index]);
}

static const char* config_sw_label = "Sync Word";

static void lora_config_sw_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_sw_names[index]);
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_sw_index = index;

    configSetSyncWord(config_sw_values[index]);
}

static const char* config_header_type_label = "Header Type";

static void lora_config_header_type_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_header_type_names[index]);
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_header_type_index = index;

    app->packetHeaderType = config_header_type_values[index];

    // Order is preamble, header type, packet length, CRC, IQ
    setPacketParams(
        app->packetPreamble,
        app->packetHeaderType,
        app->packetPayloadLength,
        app->packetCRC,
        app->packetInvertIQ);
}

static const char* config_crc_label = "CRC";

static void lora_config_crc_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_crc_names[index]);
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_crc_index = index;

    app->packetCRC = config_crc_values[index];

    // Order is preamble, header type, packet length, CRC, IQ
    setPacketParams(
        app->packetPreamble,
        app->packetHeaderType,
        app->packetPayloadLength,
        app->packetCRC,
        app->packetInvertIQ);
}

static const char* config_iq_label = "IQ";

static void lora_config_iq_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, config_iq_names[index]);
    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_iq_index = index;

    app->packetInvertIQ = config_iq_values[index];

    // Order is preamble, header type, packet length, CRC, IQ
    setPacketParams(
        app->packetPreamble,
        app->packetHeaderType,
        app->packetPayloadLength,
        app->packetCRC,
        app->packetInvertIQ);
}

static const char* config_eu_dr_label = "EU868 Data Rate";

static void lora_config_eu_dr_change(VariableItem* item) {
    VariableItem* alt_item;
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_eu_dr_names[index]);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_eu_dr_index = index;

    switch(index) {
    case 0: // SF12/125kHz
        configSetSpreadingFactor(0xC);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_sf_names[7]);
        model->config_sf_index = 7;

        break;
    case 1: // SF11/125kHz
        configSetSpreadingFactor(0x0B);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 6);
        variable_item_set_current_value_text(alt_item, config_sf_names[6]);
        model->config_sf_index = 6;

        break;
    case 2: // SF10/125kHz
        configSetSpreadingFactor(0x0A);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 5);
        variable_item_set_current_value_text(alt_item, config_sf_names[5]);
        model->config_sf_index = 5;

        break;
    case 3: // SF9/125kHz
        configSetSpreadingFactor(0x09);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 4);
        variable_item_set_current_value_text(alt_item, config_sf_names[4]);
        model->config_sf_index = 4;

        break;
    case 4: // SF8/125kHz
        configSetSpreadingFactor(0x08);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 3);
        variable_item_set_current_value_text(alt_item, config_sf_names[3]);
        model->config_sf_index = 3;

        break;
    case 5: // SF7/125kHz
        configSetSpreadingFactor(0x07);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 2);
        variable_item_set_current_value_text(alt_item, config_sf_names[2]);
        model->config_sf_index = 2;

        break;
    case 6: // SF7/250kHz
        configSetSpreadingFactor(0x07);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 1);
        variable_item_set_current_value_text(alt_item, config_sf_names[1]);
        model->config_sf_index = 1;

        break;
    }
}

static const char* config_us_dr_label = "US915 Data Rate";

static void lora_config_us_dr_change(VariableItem* item) {
    VariableItem* alt_item;
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_us_dr_names[index]);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_us_dr_index = index;

    switch(index) {
    case 0: // SF10/125kHz
        configSetSpreadingFactor(0x0A);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 5);
        variable_item_set_current_value_text(alt_item, config_sf_names[5]);
        model->config_sf_index = 5;

        break;
    case 1: // SF9/125kHz
        configSetSpreadingFactor(0x09);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 4);
        variable_item_set_current_value_text(alt_item, config_sf_names[4]);
        model->config_sf_index = 4;

        break;
    case 2: // SF8/125kHz
        configSetSpreadingFactor(0x08);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 3);
        variable_item_set_current_value_text(alt_item, config_sf_names[3]);
        model->config_sf_index = 3;

        break;
    case 3: // SF7/125kHz
        configSetSpreadingFactor(0x07);
        configSetBandwidth(0x04);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_bw_names[7]);
        model->config_bw_index = 7;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 2);
        variable_item_set_current_value_text(alt_item, config_sf_names[2]);
        model->config_sf_index = 2;

        break;
    case 4: // SF8/500kHz
        configSetSpreadingFactor(0x08);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 3);
        variable_item_set_current_value_text(alt_item, config_sf_names[3]);
        model->config_sf_index = 3;

        break;
    case 5: // SF12/500kHz
        configSetSpreadingFactor(0x0C);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 7);
        variable_item_set_current_value_text(alt_item, config_sf_names[7]);
        model->config_sf_index = 7;

        break;
    case 6: // SF11/500kHz
        configSetSpreadingFactor(0x0B);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 6);
        variable_item_set_current_value_text(alt_item, config_sf_names[6]);
        model->config_sf_index = 6;

        break;
    case 7: // SF10/500kHz
        configSetSpreadingFactor(0x0A);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 5);
        variable_item_set_current_value_text(alt_item, config_sf_names[5]);
        model->config_sf_index = 5;

        break;
    case 8: // SF9/500kHz
        configSetSpreadingFactor(0x09);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 4);
        variable_item_set_current_value_text(alt_item, config_sf_names[4]);
        model->config_sf_index = 4;

        break;
    case 9: // SF8/500kHz
        configSetSpreadingFactor(0x08);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 3);
        variable_item_set_current_value_text(alt_item, config_sf_names[3]);
        model->config_sf_index = 3;

        break;
    case 10: // SF7/500kHz
        configSetSpreadingFactor(0x07);
        configSetBandwidth(0x06);

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 1);
        variable_item_list_set_selected_item(app->variable_item_list_config, 1);
        variable_item_set_current_value_index(alt_item, 9);
        variable_item_set_current_value_text(alt_item, config_bw_names[9]);
        model->config_bw_index = 9;

        alt_item = (VariableItem*)variable_item_list_get(app->variable_item_list_config, 2);
        variable_item_list_set_selected_item(app->variable_item_list_config, 2);
        variable_item_set_current_value_index(alt_item, 2);
        variable_item_set_current_value_text(alt_item, config_sf_names[2]);
        model->config_sf_index = 2;

        break;
    }
}

static const char* config_us915_ul_channels_125k_label = "Uplink 125 kHz";

static void lora_config_us915_ul_channels_125k_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    char text_buf[11] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_us915_ul_channels_125k[index] / 1000000,
        (config_us915_ul_channels_125k[index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_us915_ul_channels_125k_index = index;

    app->config_frequency = (int)config_us915_ul_channels_125k[index];
    // setting text for configure frequency
    furi_string_set(model->config_freq_name, text_buf);
    variable_item_set_current_value_text(app->config_freq_item, text_buf);

    FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

    configSetFrequency(app->config_frequency);
}

static const char* config_us915_ul_channels_500k_label = "Uplink 500 kHz";

static void lora_config_us915_ul_channels_500k_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    char text_buf[11] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_us915_ul_channels_500k[index] / 1000000,
        (config_us915_ul_channels_500k[index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_us915_ul_channels_500k_index = index;

    app->config_frequency = (int)config_us915_ul_channels_500k[index];
    // setting text for configure frequency
    furi_string_set(model->config_freq_name, text_buf);
    variable_item_set_current_value_text(app->config_freq_item, text_buf);

    FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

    configSetFrequency(app->config_frequency);
}

static const char* config_us915_dl_channels_500k_label = "Downlink 500 kHz";

static void lora_config_us915_dl_channels_500k_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    char text_buf[11] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_us915_dl_channels_500k[index] / 1000000,
        (config_us915_dl_channels_500k[index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_us915_dl_channels_500k_index = index;

    app->config_frequency = (int)config_us915_dl_channels_500k[index];
    // setting text for configure frequency
    furi_string_set(model->config_freq_name, text_buf);
    variable_item_set_current_value_text(app->config_freq_item, text_buf);

    FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

    configSetFrequency(app->config_frequency);
}

static const char* config_eu868_ul_channels_125k_label = "Uplink 125 kHz";

static void lora_config_eu868_ul_channels_125k_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    char text_buf[11] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_eu868_ul_channels_125k[index] / 1000000,
        (config_eu868_ul_channels_125k[index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_eu868_ul_channels_125k_index = index;

    app->config_frequency = (int)config_eu868_ul_channels_125k[index];
    // setting text for configure frequency
    furi_string_set(model->config_freq_name, text_buf);
    variable_item_set_current_value_text(app->config_freq_item, text_buf);

    FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

    configSetFrequency(app->config_frequency);
}

static const char* config_eu868_ul_channels_250k_label = "Uplink 250 kHz";

static void lora_config_eu868_ul_channels_250k_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    char text_buf[11] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_eu868_ul_channels_250k[index] / 1000000,
        (config_eu868_ul_channels_250k[index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_eu868_ul_channels_250k_index = index;

    app->config_frequency = (int)config_eu868_ul_channels_250k[index];
    // setting text for configure frequency
    furi_string_set(model->config_freq_name, text_buf);
    variable_item_set_current_value_text(app->config_freq_item, text_buf);

    FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

    configSetFrequency(app->config_frequency);
}

static const char* config_eu868_dl_channels_rx1_label = "Downlink RX1";

static void lora_config_eu868_dl_channels_rx1_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    char text_buf[11] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_eu868_dl_channels_rx1[index] / 1000000,
        (config_eu868_dl_channels_rx1[index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_eu868_dl_channels_rx1_index = index;

    app->config_frequency = (int)config_eu868_dl_channels_rx1[index];
    // setting text for configure frequency
    furi_string_set(model->config_freq_name, text_buf);
    variable_item_set_current_value_text(app->config_freq_item, text_buf);

    FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

    configSetFrequency(app->config_frequency);
}

static const char* config_region_label = "Frequency Plan";

static void lora_config_region_change(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, config_region_names[index]);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    model->config_region_index = index;

    variable_item_list_reset(app->variable_item_list_lorawan);

    char text_buf[11] = {0};

    if(index == 0) {
        app->config_frequency = 868100000;
        // setting text for configure frequency
        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            app->config_frequency / 1000000,
            (app->config_frequency % 1000000) / 100000);
        variable_item_set_current_value_text(app->config_freq_item, text_buf);

        FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

        configSetFrequency(app->config_frequency);

        // Frequency Plan
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_region_label,
            COUNT_OF(config_region_values),
            lora_config_region_change,
            app);
        uint8_t config_region_index = 0;
        variable_item_set_current_value_index(item, config_region_index);
        variable_item_set_current_value_text(item, config_region_names[config_region_index]);

        // EU868 Data Rate
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_eu_dr_label,
            COUNT_OF(config_eu_dr_values),
            lora_config_eu_dr_change,
            app);
        uint8_t config_eu_dr_index = 0;
        variable_item_set_current_value_index(item, config_eu_dr_index);
        variable_item_set_current_value_text(item, config_us_dr_names[config_eu_dr_index]);

        // Uplink EU868 Channel 125K
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_eu868_ul_channels_125k_label,
            COUNT_OF(config_eu868_ul_channels_125k),
            lora_config_eu868_ul_channels_125k_change,
            app);
        uint8_t config_eu868_ul_channels_125k_index = 0;
        variable_item_set_current_value_index(item, config_eu868_ul_channels_125k_index);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            config_eu868_ul_channels_125k[index] / 1000000,
            (config_eu868_ul_channels_125k[index] % 1000000) / 100000);
        variable_item_set_current_value_text(item, text_buf);

        // Uplink EU868 Channel 250K
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_eu868_ul_channels_250k_label,
            COUNT_OF(config_eu868_ul_channels_250k),
            lora_config_eu868_ul_channels_250k_change,
            app);
        uint8_t config_eu868_ul_channels_250k_index = 0;
        variable_item_set_current_value_index(item, config_eu868_ul_channels_250k_index);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            config_eu868_ul_channels_250k[index] / 1000000,
            (config_eu868_ul_channels_250k[index] % 1000000) / 100000);
        variable_item_set_current_value_text(item, text_buf);

        // Downlink EU868 Channel RX1
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_eu868_dl_channels_rx1_label,
            COUNT_OF(config_eu868_dl_channels_rx1),
            lora_config_eu868_dl_channels_rx1_change,
            app);
        uint8_t config_eu868_dl_channels_rx1_index = 0;
        variable_item_set_current_value_index(item, config_eu868_dl_channels_rx1_index);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            config_eu868_dl_channels_rx1[index] / 1000000,
            (config_eu868_dl_channels_rx1[index] % 1000000) / 100000);
        variable_item_set_current_value_text(item, text_buf);

    } else if(index == 1) {
        app->config_frequency = 902300000; //first channel
        // setting text for configure frequency
        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            app->config_frequency / 1000000,
            (app->config_frequency % 1000000) / 100000);
        variable_item_set_current_value_text(app->config_freq_item, text_buf);

        FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);

        configSetFrequency(app->config_frequency);

        // Frequency Plan
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_region_label,
            COUNT_OF(config_region_values),
            lora_config_region_change,
            app);
        uint8_t config_region_index = 1;
        variable_item_set_current_value_index(item, config_region_index);
        variable_item_set_current_value_text(item, config_region_names[config_region_index]);

        // US915 Data Rate
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_us_dr_label,
            COUNT_OF(config_us_dr_values),
            lora_config_us_dr_change,
            app);
        uint8_t config_us_dr_index = 0;
        variable_item_set_current_value_index(item, config_us_dr_index);
        variable_item_set_current_value_text(item, config_us_dr_names[config_us_dr_index]);

        // Uplink US915 Channel 125K
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_us915_ul_channels_125k_label,
            COUNT_OF(config_us915_ul_channels_125k),
            lora_config_us915_ul_channels_125k_change,
            app);
        uint8_t config_us915_ul_channels_125k_index = 0;
        variable_item_set_current_value_index(item, config_us915_ul_channels_125k_index);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            config_us915_ul_channels_125k[index] / 1000000,
            (config_us915_ul_channels_125k[index] % 1000000) / 100000);
        variable_item_set_current_value_text(item, text_buf);

        // Uplink US915 Channel 500K
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_us915_ul_channels_500k_label,
            COUNT_OF(config_us915_ul_channels_500k),
            lora_config_us915_ul_channels_500k_change,
            app);
        uint8_t config_us915_ul_channels_500k_index = 0;
        variable_item_set_current_value_index(item, config_us915_ul_channels_500k_index);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            config_us915_ul_channels_500k[index] / 1000000,
            (config_us915_ul_channels_500k[index] % 1000000) / 100000);
        variable_item_set_current_value_text(item, text_buf);

        // Downlink US915 Channel 500K
        item = variable_item_list_add(
            app->variable_item_list_lorawan,
            config_us915_dl_channels_500k_label,
            COUNT_OF(config_us915_dl_channels_500k),
            lora_config_us915_dl_channels_500k_change,
            app);
        uint8_t config_us915_dl_channels_500k_index = 0;
        variable_item_set_current_value_index(item, config_us915_dl_channels_500k_index);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%3lu.%1lu MHz",
            config_us915_dl_channels_500k[index] / 1000000,
            (config_us915_dl_channels_500k[index] % 1000000) / 100000);
        variable_item_set_current_value_text(item, text_buf);
    }

    //configSetSpreadingFactor(config_sf_values[index]);
}

/**
 * When the user clicks OK on the configuration frequencysetting we use a text input screen to allow
 * the user to enter a frequency.  This function is called when the user clicks OK on the text input screen.
*/
static const char* config_freq_config_label = "Frequency";
static const char* config_freq_entry_text = "Enter frequency (MHz)";
static const char* config_freq_default_value = "915.0";
static void lora_config_freq_text_updated(void* context) {
    LoRaApp* app = (LoRaApp*)context;
    bool redraw = true;
    with_view_model(
        app->view_sniffer,
        LoRaSnifferModel * model,
        {
            furi_string_set(model->config_freq_name, app->temp_buffer);
            variable_item_set_current_value_text(
                app->config_freq_item, furi_string_get_cstr(model->config_freq_name));

            const char* freq_str = furi_string_get_cstr(model->config_freq_name);
            app->config_frequency = (int)(strtof(freq_str, NULL) * 1000000);
            FURI_LOG_E(TAG, "Frequency = %lu", app->config_frequency);
        },
        redraw);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewConfigure);
    configSetFrequency(app->config_frequency);
}

static void set_value(void* context) {
    LoRaApp* app = (LoRaApp*)context;

    FURI_LOG_E(TAG, "Byte buffer: %s", (char*)app->byte_buffer);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewSubmenu);
    transmit(app->byte_buffer, app->byte_buffer_size);
}

/**
 * @brief      Callback when item in configuration screen is clicked.
 * @details    This function is called when user clicks OK on an item in the configuration screen.
 *            If the item clicked is our text field then we switch to the text input screen.
 * @param      context  The context - LoRaApp object.
 * @param      index - The index of the item that was clicked.
*/
static void lora_setting_item_clicked(void* context, uint32_t index) {
    LoRaApp* app = (LoRaApp*)context;
    index++; // The index starts at zero, but we want to start at 1.

    // Our configuration UI has the 2nd item as a text field.
    if(index == 1) {
        // Header to display on the text input screen.
        text_input_set_header_text(app->frequency_input, config_freq_entry_text);

        // Copy the current name into the temporary buffer.
        bool redraw = false;
        with_view_model(
            app->view_sniffer,
            LoRaSnifferModel * model,
            {
                strncpy(
                    app->temp_buffer,
                    furi_string_get_cstr(model->config_freq_name),
                    app->temp_buffer_size);
            },
            redraw);

        // Configure the text input.  When user enters text and clicks OK, lora_setting_text_updated be called.
        bool clear_previous_text = false;
        text_input_set_result_callback(
            app->frequency_input,
            lora_config_freq_text_updated,
            app,
            app->temp_buffer,
            app->temp_buffer_size,
            clear_previous_text);

        // Pressing the BACK button will reload the configure screen.
        view_set_previous_callback(
            text_input_get_view(app->frequency_input), lora_navigation_configure_callback);

        // Show text input dialog.
        view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewFrequencyInput);
    }
}

void bytesToAsciiHex(uint8_t* buffer, uint8_t length) {
    uint8_t i;
    for(i = 0; i < length; ++i) {
        asciiBuff[i * 2] = "0123456789ABCDEF"[buffer[i] >> 4]; // High nibble
        asciiBuff[i * 2 + 1] = "0123456789ABCDEF"[buffer[i] & 0x0F]; // Low nibble
    }
    asciiBuff[length * 2] = '\0'; // Null-terminate the string
    FURI_LOG_E(TAG, "OUT bytesToAsciiHex ");
}

void asciiHexToBytes(const char* hex, uint8_t* bytes, size_t length) {
    for(size_t i = 0; i < length; i++) {
        sscanf(hex + 2 * i, "%02hhx", &bytes[i]);
    }
}

/**
 * @brief      Callback for drawing the sniffer screen.
 * @details    This function is called when the screen needs to be redrawn, like when the model gets updated.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void lora_view_sniffer_draw_callback(Canvas* canvas, void* model) {
    LoRaSnifferModel* my_model = (LoRaSnifferModel*)model;

    bool flag_file = my_model->flag_file;

    canvas_draw_icon(canvas, 0, 17, &I_flippers_cat);

    //Receive a packet over radio
    int bytesRead = lora_receive_async(receiveBuff, sizeof(receiveBuff));

    if(bytesRead > -1) {
        FURI_LOG_E(TAG, "Packet received... ");
        receiveBuff[bytesRead] = '\0';
        bytesToAsciiHex(receiveBuff, bytesRead);

        if(flag_file) {
            DateTime curr_dt;
            furi_hal_rtc_get_datetime(&curr_dt);

            char time_string[TIME_LEN];
            char date_string[DATE_LEN];

            snprintf(
                time_string,
                TIME_LEN,
                CLOCK_TIME_FORMAT,
                curr_dt.hour,
                curr_dt.minute,
                curr_dt.second);
            snprintf(
                date_string,
                DATE_LEN,
                CLOCK_ISO_DATE_FORMAT,
                curr_dt.year,
                curr_dt.month,
                curr_dt.day);

            char final_string[400];
            const char* freq_str = furi_string_get_cstr(my_model->config_freq_name);

            //JSON format
            snprintf(
                final_string,
                666,
                "{\"date\":\"%s\", \"time\":\"%s\", \"frequency\":\"%s\", \"bw\":\"%s\", \"sf\":\"%s\", \"RSSI\":\"%d\", \"payload\":\"%s\"}",
                date_string,
                time_string,
                freq_str,
                config_bw_names[my_model->config_bw_index],
                config_sf_names[my_model->config_sf_index],
                getRSSI(),
                asciiBuff);

            FURI_LOG_E(TAG, "TS: %s", final_string);
            FURI_LOG_E(TAG, "Length: %d", strlen(final_string) + 1);

            storage_file_write(my_model->file_rx, final_string, strlen(final_string));
            storage_file_write(my_model->file_rx, "\n", 1);
        }
        FURI_LOG_E(TAG, "%s", receiveBuff);
    }

    FuriString* xstr = furi_string_alloc();

    if(flag_file) {
        canvas_draw_icon(canvas, 110, 1, &I_write);
        furi_string_printf(xstr, "Recording...");
        canvas_draw_str(canvas, 60, 20, furi_string_get_cstr(xstr));
    } else {
        canvas_draw_icon(canvas, 110, 1, &I_no_write);
        furi_string_printf(xstr, "            ");
        canvas_draw_str(canvas, 60, 20, furi_string_get_cstr(xstr));
    }

    receiveBuff[17] = '.';
    receiveBuff[18] = '.';
    receiveBuff[19] = '.';
    receiveBuff[20] = '\0';

    canvas_draw_str(canvas, 1, 10, (const char*)receiveBuff);

    furi_string_printf(xstr, "RSSI: %d  ", getRSSI());
    canvas_draw_str(canvas, 1, 19, furi_string_get_cstr(xstr));

    furi_string_printf(xstr, "BW:%s", config_bw_names[my_model->config_bw_index]);
    canvas_draw_str(canvas, 1, 28, furi_string_get_cstr(xstr));

    furi_string_printf(xstr, "FQ:%s MHz", furi_string_get_cstr(my_model->config_freq_name));
    canvas_draw_str(canvas, 60, 28, furi_string_get_cstr(xstr));

    furi_string_free(xstr);
}

/**
 * @brief      Callback for drawing the transmitter screen.
 * @details    This function is called when the screen needs to be redrawn, like when the model gets updated.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void lora_view_transmitter_draw_callback(Canvas* canvas, void* model) {
    LoRaTransmitterModel* my_model = (LoRaTransmitterModel*)model;

    my_model->x = 0;

    canvas_draw_icon(canvas, 1, 3, &I_kitty_tx);

    canvas_draw_str(canvas, 1, 10, "Press central");
    canvas_draw_str(canvas, 1, 20, "button to");
    canvas_draw_str(canvas, 1, 30, "browser");

    FuriString* xstr = furi_string_alloc();
    canvas_draw_str(canvas, 1, 50, furi_string_get_cstr(xstr));
    furi_string_free(xstr);
}

/**
 * @brief      Callback for timer elapsed.
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - LoRaApp object.
*/
static void lora_view_sniffer_timer_callback(void* context) {
    LoRaApp* app = (LoRaApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaEventIdRedrawScreen);
}

/**
 * @brief      Callback for timer elapsed.
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - LoRaApp object.
*/
static void lora_view_transmitter_timer_callback(void* context) {
    LoRaApp* app = (LoRaApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LoRaEventIdRedrawScreen);
    // HERE!!!
}

/**
 * @brief      Callback when the user starts the sniffer screen.
 * @details    This function is called when the user enters the sniffer screen.  We start a timer to
 *           redraw the screen periodically (so the random number is refreshed).
 * @param      context  The context - LoRaApp object.
*/
static void lora_view_sniffer_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(1000);
    LoRaApp* app = (LoRaApp*)context;
    furi_assert(app->timer_rx == NULL);
    app->timer_rx =
        furi_timer_alloc(lora_view_sniffer_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer_rx, period);
}

/**
 * @brief      Callback when the user starts the transmitter screen.
 * @details    This function is called when the user enters the transmitter screen.  We start a timer to
 *           redraw the screen periodically (so the random number is refreshed).
 * @param      context  The context - LoRaApp object.
*/
static void lora_view_transmitter_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(1000);
    LoRaApp* app = (LoRaApp*)context;
    furi_assert(app->timer_tx == NULL);
    app->timer_tx =
        furi_timer_alloc(lora_view_transmitter_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer_tx, period);
}

/**
 * @brief      Callback when the user exits the sniffer screen.
 * @details    This function is called when the user exits the sniffer screen.  We stop the timer.
 * @param      context  The context - LoRaApp object.
*/
static void lora_view_sniffer_exit_callback(void* context) {
    LoRaApp* app = (LoRaApp*)context;
    furi_timer_stop(app->timer_rx);
    furi_timer_free(app->timer_rx);
    app->timer_rx = NULL;
    FURI_LOG_E(TAG, "Stop timer rx");
}

/**
 * @brief      Callback when the user exits the transmitter screen.
 * @details    This function is called when the user exits the transmitter screen.  We stop the timer.
 * @param      context  The context - LoRaApp object.
*/
static void lora_view_transmitter_exit_callback(void* context) {
    LoRaApp* app = (LoRaApp*)context;
    furi_timer_stop(app->timer_tx);
    furi_timer_free(app->timer_tx);
    app->timer_tx = NULL;
    FURI_LOG_E(TAG, "Stop timer tx");
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - LoRaEventId value.
 * @param      context  The context - LoRaApp object.
*/
static bool lora_view_sniffer_custom_event_callback(uint32_t event, void* context) {
    LoRaApp* app = (LoRaApp*)context;
    switch(event) {
    case LoRaEventIdRedrawScreen:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_sniffer, LoRaSnifferModel * _model, { UNUSED(_model); }, redraw);
            return true;
        }
    case LoRaEventIdOkPressed:
        // Process the OK button.  We play a tone based on the x coordinate.
        if(furi_hal_speaker_acquire(500)) {
            float frequency;
            bool redraw = false;
            with_view_model(
                app->view_sniffer,
                LoRaSnifferModel * model,
                { frequency = model->x * 100 + 100; },
                redraw);
            furi_hal_speaker_start(frequency, 1.0);
            furi_delay_ms(100);
            furi_hal_speaker_stop();
            furi_hal_speaker_release();
        }
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - LoRaEventId value.
 * @param      context  The context - LoRaApp object.
*/
static bool lora_view_transmitter_custom_event_callback(uint32_t event, void* context) {
    LoRaApp* app = (LoRaApp*)context;
    switch(event) {
    case LoRaEventIdRedrawScreen:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_transmitter, LoRaTransmitterModel * _model, { UNUSED(_model); }, redraw);
            return true;
        }
    case LoRaEventIdOkPressed:
        // Process the OK button.  We play a tone based on the x coordinate.
        if(furi_hal_speaker_acquire(500)) {
            float frequency;
            bool redraw = false;
            with_view_model(
                app->view_transmitter,
                LoRaTransmitterModel * model,
                { frequency = model->x * 100 + 100; },
                redraw);
            furi_hal_speaker_start(frequency, 1.0);
            furi_delay_ms(100);
            furi_hal_speaker_stop();
            furi_hal_speaker_release();
        }
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback for sniffer screen input.
 * @details    This function is called when the user presses a button while on the sniffer screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - LoRaApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool lora_view_sniffer_input_callback(InputEvent* event, void* context) {
    LoRaApp* app = (LoRaApp*)context;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            // Left button clicked, reduce x coordinate.
            bool redraw = true;
            with_view_model(
                app->view_sniffer,
                LoRaSnifferModel * model,
                {
                    if(model->x > 0) {
                        model->x--;
                    }
                },
                redraw);
        } else if(event->key == InputKeyRight) {
            // Right button clicked, increase x coordinate.
            bool redraw = true;
            with_view_model(
                app->view_sniffer,
                LoRaSnifferModel * model,
                {
                    // Should we have some maximum value?
                    model->x++;
                },
                redraw);
        }
    } else if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            // We choose to send a custom event when user presses OK button.  lora_custom_event_callback will
            // handle our LoRaEventIdOkPressed event.  We could have just put the code from
            // lora_custom_event_callback here, it's a matter of preference.

            bool redraw = true;
            with_view_model(
                app->view_sniffer,
                LoRaSnifferModel * model,
                {
                    // Start/Stop recording
                    model->flag_file = !model->flag_file;

                    if(model->flag_file) {
                        // if(!storage_simply_mkdir(model->storage_rx, PATHAPPEXT)) {
                        //     FURI_LOG_E(TAG, "Failed to create directory %s", PATHAPPEXT);
                        //     return;
                        // }

                        char filename[256];
                        int file_index = 0;

                        do {
                            snprintf(filename, sizeof(filename), PATHLORA, file_index);
                            file_index++;
                        } while(storage_file_exists(model->storage_rx, filename));

                        if(!storage_file_open(
                               model->file_rx, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                            FURI_LOG_E(TAG, "Failed to open file %s", filename);
                            return 0;
                        }
                        FURI_LOG_E(TAG, "OPEN FILE ");

                    } else {
                        storage_file_close(model->file_rx);
                        FURI_LOG_E(TAG, "CLOSE FILE ");
                    }
                },
                redraw);

            view_dispatcher_send_custom_event(app->view_dispatcher, LoRaEventIdOkPressed);
            return true;
        }
    }

    return false;
}

void tx_payload(const char* line) {
    const char* key = "\"payload\":\"";
    char* start = strstr(line, key);
    if(start) {
        start += strlen(key); // Advance the pointer to the end of “payload”:
        char* end = strchr(start, '"'); // find next "
        if(end) {
            // Calculates the length of the substring
            size_t length = end - start;
            char payload[length + 1]; // +1 to end in NULL
            strncpy(payload, start, length);
            payload[length] = '\0'; // adds the NULL

            FURI_LOG_E(TAG, "%s\n", payload);

            size_t byte_length = length / 2;
            uint8_t bytes[byte_length];

            // Convert hex string to bytes
            asciiHexToBytes(payload, bytes, byte_length);

            FURI_LOG_E(TAG, "%s\n", payload);
            transmit(bytes, byte_length);
            furi_delay_ms(10);
        }
    }
}

void send_data(void* context) {
    LoRaApp* app = (LoRaApp*)context;
    LoRaTransmitterModel* model = view_get_model(app->view_transmitter);

    //uint8_t transmitBuff[64];
    FuriString* predefined_filepath = furi_string_alloc_set_str(PATHAPP);
    FuriString* selected_filepath = furi_string_alloc();
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, LORA_LOG_FILE_EXTENSION, NULL);
    browser_options.base_path = PATHAPP;

    dialog_file_browser_show(
        model->dialogs_tx, selected_filepath, predefined_filepath, &browser_options);

    if(storage_file_open(
           model->file_tx,
           furi_string_get_cstr(selected_filepath),
           FSAM_READ,
           FSOM_OPEN_EXISTING)) {
        model->flag_tx_file = true;
        model->test = 1;

        char buffer[256];
        size_t buffer_index = 0;
        size_t bytes_read;
        char c;

        while((bytes_read = storage_file_read(model->file_tx, &c, 1)) > 0 && model->flag_signal) {
            if(c == '\n' || buffer_index >= 256 - 1) {
                buffer[buffer_index] = '\0';

                FURI_LOG_E(TAG, "%s\n", buffer);

                tx_payload(buffer);
                buffer_index = 0;
            } else {
                buffer[buffer_index++] = c;
            }
        }

    } else {
        dialog_message_show_storage_error(model->dialogs_tx, "Cannot open File");
    }
    storage_file_close(model->file_tx);
    model->test = 0;
    furi_string_free(selected_filepath);
    furi_string_free(predefined_filepath);

    furi_hal_gpio_write(pin_led, true);
    furi_delay_ms(50);
    furi_hal_gpio_write(pin_led, false);

    model->flag_tx_file = false;
}

/**
 * @brief      Callback for sniffer screen input.
 * @details    This function is called when the user presses a button while on the transmitter screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - LoRaApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool lora_view_transmitter_input_callback(InputEvent* event, void* context) {
    LoRaApp* app = (LoRaApp*)context;

    bool consumed = false;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        with_view_model(
            app->view_transmitter,
            LoRaTransmitterModel * model,
            {
                if(event->key == InputKeyLeft && model->test > 0) {
                    //model->test--;
                    consumed = true;
                } else if(event->key == InputKeyRight) { //&&
                    //model->test < (COUNT_OF(view_lora_tx_tests) - 1)) {
                    //model->test++;
                    consumed = true;
                } else if(event->key == InputKeyDown) { //&& model->size > 0) {
                    //model->size--;
                    consumed = true;
                } else if(event->key == InputKeyUp) { //&& model->size < 24) {
                    //model->size++;
                    consumed = true;
                } else if(event->key == InputKeyOk) {
                    uint32_t period = furi_ms_to_ticks(1000);
                    furi_timer_stop(app->timer_tx);
                    model->flag_signal = 1;
                    send_data(app);
                    furi_timer_start(app->timer_tx, period);
                    consumed = true;
                } else if(event->key == InputKeyBack) {
                    // FLAG TO STOP TRANSMISSION
                    model->flag_signal = 0;
                    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewSubmenu);
                    consumed = true;
                }
            },
            consumed);
    }

    return consumed;
}

static void lora_app_config_set_payload_length(VariableItem* item) {
    LoRaApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    FuriString* temp;
    temp = furi_string_alloc();
    furi_string_cat_printf(temp, "%d", index);
    variable_item_set_current_value_text(item, furi_string_get_cstr(temp));
    furi_string_free(temp);
    app->packetPayloadLength = index;

    app->byte_buffer_size = index;
    free(app->byte_buffer);
    app->byte_buffer = (uint8_t*)malloc(app->byte_buffer_size);

    byte_input_set_result_callback(
        app->byte_input, set_value, NULL, app, app->byte_buffer, app->byte_buffer_size);

    // Order is preamble, header type, packet length, CRC, IQ
    setPacketParams(
        app->packetPreamble,
        app->packetHeaderType,
        app->packetPayloadLength,
        app->packetCRC,
        app->packetInvertIQ);
}

/**
 * @brief      Allocate the LoRa application.
 * @details    This function allocates the LoRa application resources.
 * @return     LoRaApp object.
*/
static LoRaApp* lora_app_alloc() {
    UNUSED(lora_config_eu_dr_change);
    UNUSED(config_eu_dr_label);

    LoRaApp* app = (LoRaApp*)malloc(sizeof(LoRaApp));
    VariableItem* item;
    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Config", LoRaSubmenuIndexConfigure, lora_submenu_callback, app);
    submenu_add_item(app->submenu, "LoRaWAN", LoRaSubmenuIndexLoRaWAN, lora_submenu_callback, app);
    submenu_add_item(app->submenu, "Sniffer", LoRaSubmenuIndexSniffer, lora_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Transmitter", LoRaSubmenuIndexTransmitter, lora_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Send LoRa byte", LoRaSubmenuIndexManualTX, lora_submenu_callback, app);
    submenu_add_item(app->submenu, "About", LoRaSubmenuIndexAbout, lora_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), lora_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, LoRaViewSubmenu);

    app->frequency_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaViewFrequencyInput, text_input_get_view(app->frequency_input));
    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);

    app->byte_buffer_size = 16;
    app->byte_buffer = (uint8_t*)malloc(app->byte_buffer_size);

    app->byte_input = byte_input_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher, LoRaViewByteInput, byte_input_get_view(app->byte_input));

    byte_input_set_header_text(app->byte_input, "Set byte to LoRa TX");
    byte_input_set_result_callback(
        app->byte_input, set_value, NULL, app, app->byte_buffer, app->byte_buffer_size);

    view_set_previous_callback(
        byte_input_get_view(app->byte_input), lora_navigation_submenu_callback);

    app->packetPayloadLength = 16;

    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);

    app->variable_item_list_lorawan = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_lorawan);

    // frequency
    FuriString* config_freq_name = furi_string_alloc();
    furi_string_set_str(config_freq_name, config_freq_default_value);
    app->config_freq_item = variable_item_list_add(
        app->variable_item_list_config, config_freq_config_label, 1, NULL, NULL);
    variable_item_set_current_value_text(
        app->config_freq_item, furi_string_get_cstr(config_freq_name));

    // bw
    item = variable_item_list_add(
        app->variable_item_list_config,
        config_bw_label,
        COUNT_OF(config_bw_values),
        lora_config_bw_change,
        app);
    uint8_t config_bw_index = 7;
    variable_item_set_current_value_index(item, config_bw_index);
    variable_item_set_current_value_text(item, config_bw_names[config_bw_index]);

    // sf
    item = variable_item_list_add(
        app->variable_item_list_config,
        config_sf_label,
        COUNT_OF(config_sf_values),
        lora_config_sf_change,
        app);
    uint8_t config_sf_index = 3;
    variable_item_set_current_value_index(item, config_sf_index);
    variable_item_set_current_value_text(item, config_sf_names[config_sf_index]);

    // cr
    item = variable_item_list_add(
        app->variable_item_list_config,
        config_cr_label,
        COUNT_OF(config_cr_values),
        lora_config_cr_change,
        app);
    uint8_t config_cr_index = 0;
    variable_item_set_current_value_index(item, config_cr_index);
    variable_item_set_current_value_text(item, config_cr_names[config_cr_index]);

    // sw
    item = variable_item_list_add(
        app->variable_item_list_config,
        config_sw_label,
        COUNT_OF(config_sw_values),
        lora_config_sw_change,
        app);
    uint8_t config_sw_index = 0;
    variable_item_set_current_value_index(item, config_sw_index);
    variable_item_set_current_value_text(item, config_sw_names[config_sw_index]);

    // Payload length
    item = variable_item_list_add(
        app->variable_item_list_config,
        "Payload length",
        64,
        lora_app_config_set_payload_length,
        app);
    variable_item_set_current_value_index(item, 16);
    variable_item_set_current_value_text(item, "16");

    // Header Type
    item = variable_item_list_add(
        app->variable_item_list_config,
        config_header_type_label,
        COUNT_OF(config_header_type_values),
        lora_config_header_type_change,
        app);
    uint8_t config_header_type_index = 0;
    variable_item_set_current_value_index(item, config_header_type_index);
    variable_item_set_current_value_text(item, config_header_type_names[config_header_type_index]);

    // CRC
    item = variable_item_list_add(
        app->variable_item_list_config,
        config_crc_label,
        COUNT_OF(config_crc_values),
        lora_config_crc_change,
        app);
    uint8_t config_crc_index = 0;
    variable_item_set_current_value_index(item, config_crc_index);
    variable_item_set_current_value_text(item, config_crc_names[config_crc_index]);

    // Inverted IQ
    item = variable_item_list_add(
        app->variable_item_list_config,
        config_iq_label,
        COUNT_OF(config_iq_values),
        lora_config_iq_change,
        app);
    uint8_t config_iq_index = 0;
    variable_item_set_current_value_index(item, config_iq_index);
    variable_item_set_current_value_text(item, config_iq_names[config_iq_index]);

    // Frequency Plan
    item = variable_item_list_add(
        app->variable_item_list_lorawan,
        config_region_label,
        COUNT_OF(config_region_values),
        lora_config_region_change,
        app);
    uint8_t config_region_index = 1;
    variable_item_set_current_value_index(item, config_region_index);
    variable_item_set_current_value_text(item, config_region_names[config_region_index]);

    // Data Rate
    item = variable_item_list_add(
        app->variable_item_list_lorawan,
        config_us_dr_label,
        COUNT_OF(config_us_dr_values),
        lora_config_us_dr_change,
        app);
    uint8_t config_us_dr_index = 0;
    variable_item_set_current_value_index(item, config_us_dr_index);
    variable_item_set_current_value_text(item, config_us_dr_names[config_us_dr_index]);

    char text_buf[11] = {0};

    // Uplink Channel 125K
    item = variable_item_list_add(
        app->variable_item_list_lorawan,
        config_us915_ul_channels_125k_label,
        COUNT_OF(config_us915_ul_channels_125k),
        lora_config_us915_ul_channels_125k_change,
        app);
    uint8_t config_us915_ul_channels_125k_index = 0;
    variable_item_set_current_value_index(item, config_us915_ul_channels_125k_index);

    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_us915_ul_channels_125k[config_us915_ul_channels_125k_index] / 1000000,
        (config_us915_ul_channels_125k[config_us915_ul_channels_125k_index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    // Uplink Channel 500K
    item = variable_item_list_add(
        app->variable_item_list_lorawan,
        config_us915_ul_channels_500k_label,
        COUNT_OF(config_us915_ul_channels_500k),
        lora_config_us915_ul_channels_500k_change,
        app);
    uint8_t config_us915_ul_channels_500k_index = 0;
    variable_item_set_current_value_index(item, config_us915_ul_channels_500k_index);

    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_us915_ul_channels_500k[config_us915_ul_channels_500k_index] / 1000000,
        (config_us915_ul_channels_500k[config_us915_ul_channels_500k_index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    // Downlink Channel 500K
    item = variable_item_list_add(
        app->variable_item_list_lorawan,
        config_us915_dl_channels_500k_label,
        COUNT_OF(config_us915_dl_channels_500k),
        lora_config_us915_dl_channels_500k_change,
        app);
    uint8_t config_us915_dl_channels_500k_index = 0;
    variable_item_set_current_value_index(item, config_us915_dl_channels_500k_index);

    snprintf(
        text_buf,
        sizeof(text_buf),
        "%3lu.%1lu MHz",
        config_us915_dl_channels_500k[config_us915_dl_channels_500k_index] / 1000000,
        (config_us915_dl_channels_500k[config_us915_dl_channels_500k_index] % 1000000) / 100000);
    variable_item_set_current_value_text(item, text_buf);

    variable_item_list_set_enter_callback(
        app->variable_item_list_config, lora_setting_item_clicked, app);

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        lora_navigation_submenu_callback);

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_lorawan),
        lora_navigation_submenu_callback);

    view_dispatcher_add_view(
        app->view_dispatcher,
        LoRaViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    view_dispatcher_add_view(
        app->view_dispatcher,
        LoRaViewLoRaWAN,
        variable_item_list_get_view(app->variable_item_list_lorawan));

    app->view_sniffer = view_alloc();
    view_set_draw_callback(app->view_sniffer, lora_view_sniffer_draw_callback);
    view_set_input_callback(app->view_sniffer, lora_view_sniffer_input_callback);
    view_set_previous_callback(app->view_sniffer, lora_navigation_submenu_callback);
    view_set_enter_callback(app->view_sniffer, lora_view_sniffer_enter_callback);
    view_set_exit_callback(app->view_sniffer, lora_view_sniffer_exit_callback);
    view_set_context(app->view_sniffer, app);
    view_set_custom_callback(app->view_sniffer, lora_view_sniffer_custom_event_callback);
    view_allocate_model(app->view_sniffer, ViewModelTypeLockFree, sizeof(LoRaSnifferModel));
    LoRaSnifferModel* model_s = view_get_model(app->view_sniffer);

    model_s->config_freq_name = config_freq_name;
    model_s->config_bw_index = config_bw_index;
    model_s->config_sf_index = config_sf_index;
    model_s->config_cr_index = config_sf_index;

    model_s->config_header_type_index = config_header_type_index;
    model_s->config_crc_index = config_crc_index;
    model_s->config_iq_index = config_iq_index;

    model_s->x = 0;

    model_s->dialogs_rx = furi_record_open(RECORD_DIALOGS);
    model_s->storage_rx = furi_record_open(RECORD_STORAGE);
    model_s->file_rx = storage_file_alloc(model_s->storage_rx);

    view_dispatcher_add_view(app->view_dispatcher, LoRaViewSniffer, app->view_sniffer);

    app->view_transmitter = view_alloc();
    view_set_draw_callback(app->view_transmitter, lora_view_transmitter_draw_callback);
    view_set_input_callback(app->view_transmitter, lora_view_transmitter_input_callback);
    view_set_previous_callback(app->view_transmitter, lora_navigation_submenu_callback);
    view_set_enter_callback(app->view_transmitter, lora_view_transmitter_enter_callback);
    view_set_exit_callback(app->view_transmitter, lora_view_transmitter_exit_callback);
    view_set_context(app->view_transmitter, app);
    view_set_custom_callback(app->view_transmitter, lora_view_transmitter_custom_event_callback);
    view_allocate_model(
        app->view_transmitter, ViewModelTypeLockFree, sizeof(LoRaTransmitterModel));
    LoRaTransmitterModel* model_t = view_get_model(app->view_transmitter);

    model_t->x = 0;

    model_t->dialogs_tx = furi_record_open(RECORD_DIALOGS);
    model_t->storage_tx = furi_record_open(RECORD_STORAGE);
    model_t->file_tx = storage_file_alloc(model_t->storage_tx);

    makePaths(app);

    view_dispatcher_add_view(app->view_dispatcher, LoraViewTransmitter, app->view_transmitter);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "This is a LoRa sniffer app.\n---\nBrought to you by\nElectronicCats!\n\nauthor: @pigpen\nhttps://github.com/ElectronicCats/flipper-SX1262-LoRa");
    view_set_previous_callback(
        widget_get_view(app->widget_about), lora_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, LoRaViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    return app;
}

/**
 * @brief      Free the LoRa application.
 * @details    This function frees the LoRa application resources.
 * @param      app  The LoRa application object.
*/
static void lora_app_free(LoRaApp* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    LoRaSnifferModel* model = view_get_model(app->view_sniffer);
    storage_file_free(model->file_rx);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    LoRaTransmitterModel* model_t = view_get_model(app->view_transmitter);
    storage_file_free(model_t->file_tx);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    view_dispatcher_remove_view(app->view_dispatcher, LoRaViewFrequencyInput);
    text_input_free(app->frequency_input);

    view_dispatcher_remove_view(app->view_dispatcher, LoRaViewByteInput);
    byte_input_free(app->byte_input);

    free(app->temp_buffer);
    free(app->byte_buffer);

    view_dispatcher_remove_view(app->view_dispatcher, LoRaViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaViewSniffer);
    view_free(app->view_sniffer);
    view_dispatcher_remove_view(app->view_dispatcher, LoraViewTransmitter);
    view_free(app->view_transmitter);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaViewLoRaWAN);
    variable_item_list_free(app->variable_item_list_lorawan);
    view_dispatcher_remove_view(app->view_dispatcher, LoRaViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

/**
 * @brief      Main function for LoRa application.
 * @details    This function is the entry point for the LoRa application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t main_lora_app(void* _p) {
    UNUSED(_p);

    static FuriHalSpiBusHandle spi_handle;
    const FuriHalSpiBusHandle* spi;

    memcpy(&spi_handle, &furi_hal_spi_bus_handle_external, sizeof(FuriHalSpiBusHandle));
    spi_handle.cs = &gpio_ext_pc0;
    spi = &spi_handle;

    furi_hal_spi_bus_handle_init(spi);

    abandone();

    if(!begin()) {
        DialogsApp* dialogs_msg = furi_record_open(RECORD_DIALOGS);
        DialogMessage* message = dialog_message_alloc();
        dialog_message_set_text(
            message,
            "Error!\nSubGHz add-on module failed to start\n\nCheck that the module is plugged in",
            0,
            0,
            AlignLeft,
            AlignTop);
        dialog_message_show(dialogs_msg, message);
        dialog_message_free(message);
        furi_record_close(RECORD_DIALOGS);
        return 0;
    }

    LoRaApp* app = lora_app_alloc();

    app->packetPreamble = 0x000C;
    app->packetHeaderType = 0x00;
    app->packetPayloadLength = 0xFF;
    app->packetCRC = 0x00;
    app->packetInvertIQ = 0x00;

    view_dispatcher_run(app->view_dispatcher);

    lora_app_free(app);

    furi_hal_spi_bus_handle_deinit(spi);

    memcpy(&spi_handle, &furi_hal_spi_bus_handle_external, sizeof(FuriHalSpiBusHandle));
    spi_handle.cs = &gpio_ext_pa4;
    spi = &spi_handle;

    // Typically when a pin is no longer in use, it is set to analog mode.
    furi_hal_gpio_init_simple(pin_led, GpioModeAnalog);

    return 0;
}
