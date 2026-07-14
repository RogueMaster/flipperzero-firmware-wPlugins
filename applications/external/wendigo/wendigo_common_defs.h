#ifndef WENDIGO_COMMON_DEFS
#define WENDIGO_COMMON_DEFS

/**
 * Structs have just blown my mind - based on its members wendigo_bt_device
 * (excluding wendigo_bt_svc) should be 45 bytes but it's 56. Wendigo_bt_svc
 * should be 10, is 16. I guess they're not just a chunk of memory containing
 * their members.
 * This means that a wendigo_bt_device can't be reliably sent and reconstructed
 * as a whole. So instead we'll pull the individual attributes out of the buffer
 * based on their offsets. This file defines their offsets into a packet, with
 * offset 0 representing the first byte of the packet preamble.
 * 
 */
#define WENDIGO_OFFSET_BT_BDNAME_LEN         (4)
#define WENDIGO_OFFSET_BT_EIR_LEN            (5)
#define WENDIGO_OFFSET_BT_RSSI               (6)
#define WENDIGO_OFFSET_BT_COD                (8)
#define WENDIGO_OFFSET_BT_BDA                (12)
#define WENDIGO_OFFSET_BT_SCANTYPE           (18)
#define WENDIGO_OFFSET_BT_TAGGED             (19)
#define WENDIGO_OFFSET_BT_LASTSEEN           (20)
#define WENDIGO_OFFSET_BT_NUM_SERVICES       (39)
#define WENDIGO_OFFSET_BT_KNOWN_SERVICES_LEN (40)
#define WENDIGO_OFFSET_BT_COD_LEN            (41)
#define WENDIGO_OFFSET_BT_BDNAME             (42)
/* bdname is bdname_len bytes, followed by eir_len bytes of EIR and cod_len bytes of CoD */

/* Initial elements of AP and STA packets are common so are just defined once */
#define WENDIGO_OFFSET_WIFI_SCANTYPE   (4)
#define WENDIGO_OFFSET_WIFI_MAC        (5)
#define WENDIGO_OFFSET_WIFI_CHANNEL    (11)
#define WENDIGO_OFFSET_WIFI_RSSI       (12)
#define WENDIGO_OFFSET_WIFI_LASTSEEN   (14)
#define WENDIGO_OFFSET_WIFI_TAGGED     (33)
/* Unique elements */
#define WENDIGO_OFFSET_STA_PNL_COUNT   (34)
#define WENDIGO_OFFSET_STA_AP_MAC      (35)
#define WENDIGO_OFFSET_STA_AP_SSID_LEN (41)
#define WENDIGO_OFFSET_STA_AP_SSID     (42)
/* SSID is SSID_Len bytes, followed by WENDIGO_OFFSET_STA_PNL_COUNT sets of SSIDs,
    where each SSID is 1 byte for SSID length followed by that number of bytes for
    the SSID. Finally, this is followed by the packet terminator which is
    PREAMBLE_LEN == 4 bytes */
#define WENDIGO_OFFSET_AP_AUTH_MODE    (34)
#define WENDIGO_OFFSET_AP_SSID_LEN     (35)
#define WENDIGO_OFFSET_AP_STA_COUNT    (36)
#define WENDIGO_OFFSET_AP_SSID         (37)
/* SSID is SSID_Len bytes. Each station is a 6-byte MAC. There are STA_COUNT stations. */

#define WENDIGO_OFFSET_CHANNEL_COUNT (4)
#define WENDIGO_OFFSET_CHANNELS      (5)

#define WENDIGO_OFFSET_MAC_IF_COUNT   (4)
#define WENDIGO_OFFSET_MAC_BT_TYPE    (5)
#define WENDIGO_OFFSET_MAC_BT_MAC     (6)
#define WENDIGO_OFFSET_MAC_WIFI_TYPE  (12)
#define WENDIGO_OFFSET_MAC_WIFI_MAC   (13)
#define WENDIGO_OFFSET_MAC_TERMINATOR (19)

#ifdef IS_FLIPPER_APP
typedef enum {
    WIFI_AUTH_OPEN = 0,
    WIFI_AUTH_WEP = 1,
    WIFI_AUTH_WPA_PSK = 2,
    WIFI_AUTH_WPA2_PSK = 3,
    WIFI_AUTH_WPA_WPA2_PSK = 4,
    WIFI_AUTH_ENTERPRISE = 5, /**< Wi-Fi EAP security */
    WIFI_AUTH_WPA2_ENTERPRISE = 6, /**< Wi-Fi EAP security */
    WIFI_AUTH_WPA3_PSK = 7,
    WIFI_AUTH_WPA2_WPA3_PSK = 8,
    WIFI_AUTH_WAPI_PSK = 9,
    WIFI_AUTH_OWE = 10,
    WIFI_AUTH_WPA3_ENT_192 = 11, /**< WPA3_ENT_SUITE_B_192_BIT */
    WIFI_AUTH_WPA3_EXT_PSK = 12, /**< This authentication mode will yield
                the same result as WIFI_AUTH_WPA3_PSK and is not recommended for use. It
                will be deprecated in future, please use WIFI_AUTH_WPA3_PSK instead. */
    WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE = 13, /**< This authentication mode will yield
                the same result as WIFI_AUTH_WPA3_PSK and is not recommended for use. It
                will be deprecated in future, please use WIFI_AUTH_WPA3_PSK instead. */
    WIFI_AUTH_DPP = 14,
    WIFI_AUTH_WPA3_ENTERPRISE = 15, /**< WPA3-Enterprise Only Mode */
    WIFI_AUTH_WPA2_WPA3_ENTERPRISE = 16, /**< WPA3-Enterprise Transition Mode */
    WIFI_AUTH_MAX = 17
} wifi_auth_mode_t;

#define WENDIGO_TAG "WENDIGO"
#endif

/* A warning about this being defined in ESP-IDF appeared out of nowhere */
#ifndef MAX_SSID_LEN
#define MAX_SSID_LEN (32)
#endif
#define MAC_STRLEN (17)
#define MAC_BYTES  (6)

/* enum ScanType being replaced with uint8_t */
extern const uint8_t SCAN_HCI;
extern const uint8_t SCAN_BLE;
extern const uint8_t SCAN_WIFI_AP;
extern const uint8_t SCAN_WIFI_STA;
extern const uint8_t SCAN_INTERACTIVE;
extern const uint8_t SCAN_TAG;
extern const uint8_t SCAN_FOCUS;
extern const uint8_t SCAN_COUNT;
/* But I want to use SCAN_COUNT for array declarations - How annoying */
#define DEF_SCAN_COUNT (7)

typedef enum DeviceMask {
    DEVICE_BT_CLASSIC = 1,
    DEVICE_BT_LE = 2,
    DEVICE_WIFI_AP = 4,
    DEVICE_WIFI_STA = 8,
    DEVICE_SELECTED_ONLY = 16,
    DEVICE_CUSTOM = 32,
    DEVICE_ALL = 15
} DeviceMask;

/** Enum bitmask that defines supported hardware features */
typedef enum SupportedHardwareMask {
    HW_WIFI_24_SUPPORTED = 1,
    HW_WIFI_5_SUPPORTED = 2,
    HW_BT_CLASSIC_SUPPORTED = 4,
    HW_BLE_SUPPORTED = 8,
    HW_BT_UUID_DICTIONARY = 16,
    /* If logical-and with the following values is non-zero they're supported */
    HW_WIFI_SUPPORTED = 3,
    HW_BT_SUPPORTED = 12
} SupportedHardwareMask;

/** Enum to specify MACs for retrieval and updating */
typedef enum {
    WENDIGO_MAC_BASE = 0,
    WENDIGO_MAC_WIFI,
    WENDIGO_MAC_BLUETOOTH,
    WENDIGO_MACS_COUNT
} WendigoMAC;

typedef struct {
    uint16_t uuid16;
    char name[40];
} bt_uuid;

typedef struct {
    uint8_t num_services;
#ifdef IS_FLIPPER_APP
    void* service_uuids; // Was esp_bt_uuid_t
#else
    esp_bt_uuid_t* service_uuids;
#endif
    bt_uuid** known_services;
    uint8_t known_services_len;
} wendigo_bt_svc;

typedef struct {
    uint8_t bdname_len;
    uint8_t eir_len;
    uint32_t cod;
    char* cod_str;
    uint8_t* eir; // Consider inline - [ESP_BT_GAP_EIR_DATA_LEN]
    char* bdname; // Consider inline - [ESP_BT_GAP_MAX_BDNAME_LEN + 1]
    wendigo_bt_svc bt_services;
} wendigo_bt_device;

typedef struct wendigo_wifi_ap {
    uint8_t** stations; /** array of MACs */
    uint8_t stations_count; /** Count of devices in stations */
    char ssid[MAX_SSID_LEN + 1]; /** SSID of AP */
    uint8_t channel;
    uint8_t authmode; /** Was wifi_auth_mode_t - Change to uint8_t to manage storage */
    uint32_t phy_11b  : 1; /**< Bit: 0 flag to identify if 11b mode is enabled or not */
    uint32_t phy_11g  : 1; /**< Bit: 1 flag to identify if 11g mode is enabled or not */
    uint32_t phy_11n  : 1; /**< Bit: 2 flag to identify if 11n mode is enabled or not */
    uint32_t phy_lr   : 1; /**< Bit: 3 flag to identify if low rate is enabled or not */
    uint32_t phy_11a  : 1; /**< Bit: 4 flag to identify if 11ax mode is enabled or not */
    uint32_t phy_11ac : 1; /**< Bit: 5 flag to identify if 11ax mode is enabled or not */
    uint32_t phy_11ax : 1; /**< Bit: 6 flag to identify if 11ax mode is enabled or not */
    uint32_t wps      : 1; /**< Bit: 7 flag to identify if WPS is supported or not */
    uint32_t ftm_responder : 1; /**< Bit: 8 flag to identify if FTM is supported in responder mode */
    uint32_t ftm_initiator : 1; /**< Bit: 9 flag to identify if FTM is supported in initiator mode */
    uint32_t reserved : 22; /**< Bit: 10..31 reserved */
} wendigo_wifi_ap;

typedef struct wendigo_wifi_sta {
    uint8_t apMac[MAC_BYTES];
    uint8_t channel;
    uint8_t saved_networks_count;
    char** saved_networks;
} wendigo_wifi_sta;

typedef struct wendigo_device {
    uint8_t mac[MAC_BYTES];
    int16_t rssi;
    uint8_t scanType;
    bool tagged;
#ifdef IS_FLIPPER_APP
    uint32_t lastSeen;
    VariableItem* view;
#else
    struct timeval lastSeen;
#endif
    union {
        wendigo_bt_device bluetooth;
        wendigo_wifi_ap ap;
        wendigo_wifi_sta sta;
    } radio;
} wendigo_device;

extern uint8_t PREAMBLE_LEN;
extern uint8_t PREAMBLE_BT_BLE[];
extern uint8_t PREAMBLE_WIFI_AP[];
extern uint8_t PREAMBLE_WIFI_STA[];
extern uint8_t PREAMBLE_CHANNELS[];
extern uint8_t PREAMBLE_STATUS[];
extern uint8_t PREAMBLE_VER[];
extern uint8_t PREAMBLE_MAC[];
extern uint8_t PACKET_TERM[];
extern uint8_t nullMac[];
extern uint8_t broadcastMac[];

#endif
