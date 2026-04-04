#include "dhcp_fingerprint.h"
#include "../utils/packet_utils.h"
#include <string.h>

#define ETH_TYPE_IPV4     0x0800
#define IP_PROTO_UDP      17
#define DHCP_CLIENT_PORT  68
#define DHCP_SERVER_PORT  67
#define DHCP_MAGIC_COOKIE 0x63825363

/* DHCP message types */
#define DHCP_DISCOVER 1
#define DHCP_REQUEST  3

void dhcp_fp_init(DhcpFpState* state) {
    memset(state, 0, sizeof(DhcpFpState));
}

/**
 * Known OS fingerprints based on option 55 parameter request list.
 * Format: array of option values, terminated by 0.
 * These are common patterns seen in the wild.
 */
typedef struct {
    const uint8_t options[16];
    uint8_t count;
    const char* os_name;
} DhcpFpSignature;

static const DhcpFpSignature known_signatures[] = {
    /* Windows 10/11 */
    {{1, 3, 6, 15, 31, 33, 43, 44, 46, 47, 119, 121, 249, 252}, 14, "Windows 10/11"},
    /* Windows 7/8 */
    {{1, 3, 6, 15, 31, 33, 43, 44, 46, 47, 119, 121, 249, 252}, 14, "Windows 7/8+"},
    /* macOS / iOS */
    {{1, 121, 3, 6, 15, 119, 252}, 7, "macOS/iOS"},
    {{1, 121, 3, 6, 15, 119, 252, 95, 44, 46}, 10, "macOS"},
    /* Linux (dhclient) */
    {{1, 28, 2, 3, 15, 6, 119, 12, 44, 47, 26, 121, 42}, 13, "Linux (dhclient)"},
    /* Linux (NetworkManager) */
    {{1, 28, 2, 121, 15, 6, 12, 40, 41, 42, 26, 119, 3}, 13, "Linux (NM)"},
    /* Android */
    {{1, 3, 6, 15, 26, 28, 51, 58, 59, 43}, 10, "Android"},
    /* Cisco IP Phone */
    {{1, 66, 6, 3, 15, 150, 35}, 7, "Cisco IP Phone"},
    /* Printer (generic) */
    {{1, 3, 6, 15, 44, 47}, 6, "Printer"},
    /* Raspberry Pi / Debian */
    {{1, 28, 2, 3, 15, 6, 12}, 7, "Debian/RPi"},
    /* FreeBSD */
    {{1, 28, 2, 3, 15, 6, 12, 44}, 8, "FreeBSD"},
    /* VMware */
    {{1, 3, 6, 15, 28, 42}, 6, "VMware"},
    /* ChromeOS */
    {{1, 121, 3, 6, 15, 114, 119}, 7, "ChromeOS"},
};

#define NUM_SIGNATURES (sizeof(known_signatures) / sizeof(known_signatures[0]))

/**
 * Match option 55 list against known signatures.
 * Uses a scoring system: exact prefix match gets highest score.
 */
static const char* dhcp_fp_identify(const uint8_t* options, uint8_t count) {
    int best_score = 0;
    const char* best_match = "Unknown";

    for(uint16_t i = 0; i < NUM_SIGNATURES; i++) {
        const DhcpFpSignature* sig = &known_signatures[i];
        int score = 0;

        /* Count matching options in order */
        uint8_t min_len = count < sig->count ? count : sig->count;
        for(uint8_t j = 0; j < min_len; j++) {
            if(options[j] == sig->options[j]) {
                score += 2; /* position match */
            }
        }

        /* Count matching options regardless of order */
        for(uint8_t j = 0; j < count; j++) {
            for(uint8_t k = 0; k < sig->count; k++) {
                if(options[j] == sig->options[k]) {
                    score += 1;
                    break;
                }
            }
        }

        /* Penalize length mismatch */
        int len_diff = (int)count - (int)sig->count;
        if(len_diff < 0) len_diff = -len_diff;
        score -= len_diff;

        if(score > best_score) {
            best_score = score;
            best_match = sig->os_name;
        }
    }

    /* Require minimum confidence */
    if(best_score < 6) return "Unknown";
    return best_match;
}

/**
 * Find client by MAC or create new entry.
 */
static DhcpFpClient* dhcp_fp_find_or_create(DhcpFpState* state, const uint8_t mac[6]) {
    for(uint16_t i = 0; i < state->client_count; i++) {
        if(memcmp(state->clients[i].mac, mac, 6) == 0) {
            return &state->clients[i];
        }
    }
    if(state->client_count < DHCP_FP_MAX_CLIENTS) {
        DhcpFpClient* c = &state->clients[state->client_count];
        memset(c, 0, sizeof(DhcpFpClient));
        memcpy(c->mac, mac, 6);
        state->client_count++;
        return c;
    }
    return NULL;
}

bool dhcp_fp_process_frame(DhcpFpState* state, const uint8_t* frame, uint16_t len) {
    if(len < 14) return false;

    uint16_t ethertype = pkt_get_ethertype(frame);
    if(ethertype != ETH_TYPE_IPV4) return false;

    /* IPv4 header */
    const uint8_t* ip = frame + 14;
    if(len < 14 + 20) return false;
    uint8_t ihl = (ip[0] & 0x0F) * 4;
    if(ip[9] != IP_PROTO_UDP) return false;

    /* UDP header */
    if(len < (uint16_t)(14 + ihl + 8)) return false;
    const uint8_t* udp = ip + ihl;
    uint16_t src_port = pkt_read_u16_be(&udp[0]);
    uint16_t dst_port = pkt_read_u16_be(&udp[2]);

    /* DHCP: client port 68 → server port 67 */
    if(src_port != DHCP_CLIENT_PORT && dst_port != DHCP_SERVER_PORT) return false;

    /* BOOTP/DHCP starts after UDP header */
    const uint8_t* bootp = udp + 8;
    uint16_t bootp_offset = 14 + ihl + 8;
    if(len < bootp_offset + 240) return false;

    /* Check BOOTP op = request (1) */
    if(bootp[0] != 1) return false;

    /* Check magic cookie */
    if(pkt_read_u32_be(&bootp[236]) != DHCP_MAGIC_COOKIE) return false;

    /* Parse DHCP options */
    uint16_t opt_idx = 240;
    uint8_t msg_type = 0;
    uint8_t opt55[DHCP_FP_MAX_OPTIONS];
    uint8_t opt55_len = 0;

    while(bootp_offset + opt_idx < len && bootp[opt_idx] != 255) {
        if(bootp[opt_idx] == 0) {
            opt_idx++;
            continue;
        }
        uint8_t opt = bootp[opt_idx++];
        if(bootp_offset + opt_idx >= len) break;
        uint8_t olen = bootp[opt_idx++];
        if(bootp_offset + opt_idx + olen > len) break;

        if(opt == 53 && olen >= 1) {
            msg_type = bootp[opt_idx];
        }
        if(opt == 55 && olen > 0) {
            opt55_len = olen < DHCP_FP_MAX_OPTIONS ? olen : DHCP_FP_MAX_OPTIONS;
            memcpy(opt55, &bootp[opt_idx], opt55_len);
        }

        opt_idx += olen;
    }

    /* Only fingerprint Discover and Request messages */
    if(msg_type != DHCP_DISCOVER && msg_type != DHCP_REQUEST) return false;
    if(opt55_len == 0) return false;

    state->total_discovers++;

    /* Get source MAC from BOOTP chaddr (more reliable than Ethernet header) */
    uint8_t client_mac[6];
    memcpy(client_mac, &bootp[28], 6);

    DhcpFpClient* client = dhcp_fp_find_or_create(state, client_mac);
    if(!client) return false;

    if(client->identified) return false; /* Already fingerprinted */

    memcpy(client->options, opt55, opt55_len);
    client->option_count = opt55_len;

    const char* os = dhcp_fp_identify(opt55, opt55_len);
    strncpy(client->os_guess, os, sizeof(client->os_guess) - 1);
    client->os_guess[sizeof(client->os_guess) - 1] = '\0';
    client->identified = true;

    return true;
}
