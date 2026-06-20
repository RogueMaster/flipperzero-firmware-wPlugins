#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SNMP_MAX_STRING 128

typedef struct {
    char sys_name[SNMP_MAX_STRING];
    char sys_descr[SNMP_MAX_STRING];
    uint32_t sys_uptime; /* in hundredths of a second */
    int32_t if_oper_status; /* 1=up, 2=down, 3=testing, -1=error */
    bool has_sys_name;
    bool has_sys_descr;
    bool has_sys_uptime;
    bool has_if_status;
    bool valid;
} SnmpGetResult;

/**
 * Send SNMP v1/v2c GET requests for sysName, sysDescr, sysUpTime, ifOperStatus.
 * @param target_ip  Target device IP
 * @param community  Community string (e.g. "public")
 * @param use_v2c    true for SNMPv2c, false for SNMPv1
 * @param result     Output structure
 * @return true if at least one OID was retrieved
 */
bool snmp_client_get(
    const uint8_t target_ip[4],
    const char* community,
    bool use_v2c,
    SnmpGetResult* result);
