#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DNS_POISON_MAX_ADDRS 4

typedef struct {
    uint8_t local_addrs[DNS_POISON_MAX_ADDRS][4];
    uint8_t local_count;
    uint8_t public_addrs[DNS_POISON_MAX_ADDRS][4];
    uint8_t public_count;
    bool match; /* true if results overlap */
    bool mismatch; /* true if results completely differ */
    bool local_ok; /* local DNS responded */
    bool public_ok; /* public DNS responded */
    bool valid;
} DnsPoisonResult;

/**
 * Compare DNS resolution results between local and public DNS servers.
 * @param hostname     Hostname to resolve
 * @param local_dns    Local DNS server IP (from DHCP)
 * @param public_dns   Public DNS server IP (e.g. 8.8.8.8)
 * @param result       Output comparison result
 * @return true if at least one server responded
 */
bool dns_poison_check(
    const char* hostname,
    const uint8_t local_dns[4],
    const uint8_t public_dns[4],
    DnsPoisonResult* result);
