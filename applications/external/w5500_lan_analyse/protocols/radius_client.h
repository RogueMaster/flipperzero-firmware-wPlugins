#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t code; /* 2=Accept, 3=Reject, 11=Challenge */
    uint8_t identifier;
    uint16_t length;
    bool response_received;
    char status_str[32]; /* "Access-Accept", "Access-Reject", etc. */
    bool valid;
} RadiusResult;

/**
 * Send RADIUS Access-Request and check response.
 * Uses PAP authentication with MD5 password hiding per RFC 2865.
 * @param server_ip   RADIUS server IP
 * @param secret      Shared secret
 * @param username     Username to test
 * @param password     Password to test
 * @param result       Output result
 * @return true if server responded
 */
bool radius_test(
    const uint8_t server_ip[4],
    const char* secret,
    const char* username,
    const char* password,
    RadiusResult* result);
