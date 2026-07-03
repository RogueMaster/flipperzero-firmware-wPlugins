#pragma once

#include <stdint.h>
#include <stdbool.h>

#define NETBIOS_MAX_NAMES 16

typedef struct {
    char name[16]; /* NetBIOS name (15 chars + null) */
    uint8_t suffix; /* name suffix (type byte) */
    uint16_t flags; /* name flags */
    bool is_group; /* group name flag */
} NetbiosName;

typedef struct {
    NetbiosName names[NETBIOS_MAX_NAMES];
    uint8_t name_count;
    uint8_t unit_id[6]; /* MAC address from adapter status */
    bool has_unit_id;
    char computer_name[16]; /* first unique name (workstation) */
    char workgroup[16]; /* first group name */
    bool valid;
} NetbiosQueryResult;

/**
 * Send NetBIOS Node Status Request (NBSTAT) to target IP.
 * @param target_ip  Target device IP
 * @param result     Output structure with names and workgroup
 * @return true if valid response received
 */
bool netbios_node_status(const uint8_t target_ip[4], NetbiosQueryResult* result);
