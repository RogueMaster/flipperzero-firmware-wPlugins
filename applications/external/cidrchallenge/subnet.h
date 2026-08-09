#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SUBNET_IP_STR_SIZE 16

/** Netmask for a prefix length, e.g. 27 -> 0xFFFFFFE0. */
uint32_t subnet_mask(uint8_t prefix);

/** Network (wire) address of the block containing ip. */
uint32_t subnet_network(uint32_t ip, uint8_t prefix);

/** Broadcast address of the block containing ip. */
uint32_t subnet_broadcast(uint32_t ip, uint8_t prefix);

/** First usable host. /31 returns the network itself, /32 the address. */
uint32_t subnet_first_host(uint32_t ip, uint8_t prefix);

/** Last usable host. /31 returns network + 1, /32 the address. */
uint32_t subnet_last_host(uint32_t ip, uint8_t prefix);

/** Usable hosts: 2^n - 2, with /31 = 2 (RFC 3021) and /32 = 1. */
uint32_t subnet_usable_hosts(uint8_t prefix);

/** Addresses in the block, saturated at UINT32_MAX for /0. */
uint32_t subnet_block_size(uint8_t prefix);

/** Smallest prefix (largest block is /8) able to host the requested count. */
uint8_t subnet_prefix_for_hosts(uint32_t hosts);

/** Prefix length of a contiguous netmask, or 255 if the mask is invalid. */
uint8_t subnet_prefix_from_mask(uint32_t mask);

void subnet_format_ip(uint32_t ip, char* out, size_t out_size);
