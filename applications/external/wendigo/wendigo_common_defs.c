#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

/* We can determine whether it's the ESP32 app by checking for an ESP-IDF target device */
#if defined(CONFIG_IDF_TARGET)
#include <esp_bt_defs.h>
#include <esp_wifi_types.h>
#else
#include <gui/modules/variable_item_list.h>
#define IS_FLIPPER_APP (1)
#endif

#include "wendigo_common_defs.h"

/* Packet identifiers */
uint8_t PREAMBLE_LEN = 4;
uint8_t PREAMBLE_BT_BLE[] = {0xFF, 0xFE, 0xFD, 0xFC};
uint8_t PREAMBLE_WIFI_AP[] = {0x99, 0x98, 0x97, 0x96};
uint8_t PREAMBLE_WIFI_STA[] = {0x88, 0x87, 0x86, 0x85};
uint8_t PREAMBLE_CHANNELS[] = {0x77, 0x76, 0x75, 0x74};
uint8_t PREAMBLE_STATUS[] = {0x66, 0x65, 0x64, 0x63};
uint8_t PREAMBLE_VER[] = {'W', 'e', 'n', 'd'};
uint8_t PREAMBLE_MAC[] = {0x55, 0x54, 0x53, 0x52};
uint8_t PACKET_TERM[] = {0xAA, 0xBB, 0xCC, 0xDD};

uint8_t nullMac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* enum ScanType being replaced with uint8_t */
const uint8_t SCAN_HCI = 0;
const uint8_t SCAN_BLE = 1;
const uint8_t SCAN_WIFI_AP = 2;
const uint8_t SCAN_WIFI_STA = 3;
const uint8_t SCAN_INTERACTIVE = 4;
const uint8_t SCAN_TAG = 5;
const uint8_t SCAN_FOCUS = 6;
const uint8_t SCAN_COUNT = 7;
