#pragma once

#include <stdint.h>
#include <stdbool.h>

/* IPMI Chassis status bits */
#define IPMI_CHASSIS_POWER_ON     0x01
#define IPMI_CHASSIS_OVERLOAD     0x02
#define IPMI_CHASSIS_FAULT        0x08
#define IPMI_CHASSIS_POWER_POLICY 0x60

typedef struct {
    /* Chassis Status */
    uint8_t power_state; /* bit 0 = power on */
    bool chassis_ok;

    /* Device ID */
    uint8_t device_id;
    uint8_t device_revision;
    uint8_t firmware_major;
    uint8_t firmware_minor;
    uint8_t ipmi_version;
    bool device_ok;

    bool valid;
    char error_msg[48];
} IpmiResult;

/**
 * Query IPMI v1.5 over LAN (unauthenticated session).
 * Sends Get Chassis Status and Get Device ID commands.
 * @param target_ip  BMC IP address
 * @param result     Output result
 * @return true if BMC responded
 */
bool ipmi_query(const uint8_t target_ip[4], IpmiResult* result);
