#ifndef WENDIGO_PNL_H
#define WENDIGO_PNL_H

/** PreferredNetwork is used to allow us to browse from SSID to devices
 * that have probed for that SSID.
 */
typedef struct PreferredNetwork {
    char ssid[MAX_SSID_LEN + 1];
    uint8_t device_count;
    wendigo_device** devices;
} PreferredNetwork;

typedef enum PNL_Result {
    PNL_EXISTS,
    PNL_CREATED,
    PNL_DEVICE_CREATED,
    PNL_FAILED,
    PNL_IN_PROGRESS
} PNL_Result;

uint16_t index_of_pnl(char* ssid);
PreferredNetwork* pnl_for_ssid(char* ssid);
uint8_t count_networks_for_device(wendigo_device* dev);
uint16_t map_ssids_to_devices(WendigoApp* app);
uint8_t get_networks_for_device(WendigoApp* app, wendigo_device* dev, char*** result);
uint16_t get_all_networks(WendigoApp* app);
PreferredNetwork* fetch_or_create_pnl(char* ssid, PNL_Result* result);
uint8_t pnl_index_of_device(PreferredNetwork* pnl, wendigo_device* dev);
uint8_t pnl_index_of_mac(PreferredNetwork* pnl, uint8_t mac[MAC_BYTES]);
PNL_Result pnl_find_or_create_device(WendigoApp* app, char* ssid, wendigo_device* dev);
void pnl_log_result(char* tag, PNL_Result res, char* ssid, wendigo_device* dev);

/* Preferred Network List caches */
extern PreferredNetwork* networks;
extern uint16_t networks_count;
extern uint16_t networks_capacity;

#endif
