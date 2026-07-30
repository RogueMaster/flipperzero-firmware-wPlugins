#pragma once

#include <furi.h>

#define WOL_NAME_LEN    24
#define WOL_IP_LEN      16
#define WOL_SSID_LEN    33
#define WOL_PASS_LEN    65
#define WOL_MAX_TARGETS 16
#define WOL_PACKET_SIZE 102

typedef struct {
    char name[WOL_NAME_LEN];
    uint8_t mac[6];
    char ip[WOL_IP_LEN];
    uint16_t port;
} WolTarget;

typedef struct {
    char ssid[WOL_SSID_LEN];
    char pass[WOL_PASS_LEN];
    WolTarget targets[WOL_MAX_TARGETS];
    uint8_t target_count;
} WolConfig;

/** snprintf based, always NUL terminated string copy (no strlcpy in the SDK libc). */
void wol_strcpy(char* dst, size_t dst_len, const char* src);

/** Fill cfg with defaults, then overwrite from disk if a config file exists. */
void wol_config_load(WolConfig* cfg);

/** Persist cfg. Returns false on storage error. */
bool wol_config_save(const WolConfig* cfg);

/** Reset a target to sane defaults (empty name, zero mac, 255.255.255.255:9). */
void wol_target_default(WolTarget* target);

/** Format mac as AA:BB:CC:DD:EE:FF into out (needs 18 bytes). */
void wol_mac_to_str(const uint8_t* mac, char* out, size_t out_len);

/** Build the 102 byte magic packet: 6x 0xFF followed by 16 repeats of mac. */
void wol_build_magic_packet(const uint8_t* mac, uint8_t* out);

/** Loose sanity check of a dotted-quad address. */
bool wol_ip_is_valid(const char* ip);
