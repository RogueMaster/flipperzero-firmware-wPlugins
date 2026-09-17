#ifndef GHOST_CONFIG_AP_LAYOUT_H
#define GHOST_CONFIG_AP_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#define GHOST_CONFIG_AP_NO_ROW UINT32_MAX

typedef struct {
    uint32_t ap_name;
    uint32_t security;
    uint32_t password;
    uint32_t channel;
    uint32_t portal_type;
    uint32_t portal_file;
    uint32_t mac_type;
    uint32_t custom_mac;
    uint32_t start;
    uint32_t count;
} GhostConfigApLayout;

static inline GhostConfigApLayout
    ghost_config_ap_layout(bool wpa, bool custom_mac, bool custom_portal) {
    GhostConfigApLayout layout = {
        .password = GHOST_CONFIG_AP_NO_ROW,
        .portal_file = GHOST_CONFIG_AP_NO_ROW,
        .custom_mac = GHOST_CONFIG_AP_NO_ROW,
    };
    uint32_t row = 0;

    layout.ap_name = row++;
    layout.security = row++;
    if(wpa) layout.password = row++;
    layout.channel = row++;
    layout.portal_type = row++;
    if(custom_portal) layout.portal_file = row++;
    layout.mac_type = row++;
    if(custom_mac) layout.custom_mac = row++;
    layout.start = row++;
    layout.count = row;

    return layout;
}

#endif
