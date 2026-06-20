#include "cdp.h"
#include "../utils/packet_utils.h"

#include <furi.h>
#include <string.h>

#define TAG "CDP"

uint16_t cdp_check_frame(const uint8_t* frame, uint16_t frame_len) {
    /*
     * CDP uses LLC/SNAP encapsulation after the Ethernet header.
     * Ethernet header (14 bytes):
     *   6 bytes dst MAC
     *   6 bytes src MAC
     *   2 bytes length (NOT EtherType, since < 0x0600)
     * LLC header (3 bytes):
     *   DSAP=0xAA, SSAP=0xAA, Control=0x03
     * SNAP header (5 bytes):
     *   OUI = 00-00-0C
     *   Type = 0x2000
     *
     * Total overhead = 14 + 3 + 5 = 22 bytes before CDP payload.
     */
    if(frame_len < 22) return 0;

    /* Check destination MAC */
    const uint8_t cdp_mac[] = CDP_DST_MAC;
    if(memcmp(frame, cdp_mac, 6) != 0) return 0;

    /* Check LLC/SNAP header starting at offset 14 */
    if(frame[14] != CDP_SNAP_DSAP) return 0;
    if(frame[15] != CDP_SNAP_SSAP) return 0;
    if(frame[16] != CDP_SNAP_CTRL) return 0;

    /* Check SNAP OUI (00-00-0C = Cisco) */
    if(frame[17] != CDP_SNAP_OUI_0) return 0;
    if(frame[18] != CDP_SNAP_OUI_1) return 0;
    if(frame[19] != CDP_SNAP_OUI_2) return 0;

    /* Check SNAP Type (0x2000 = CDP) */
    uint16_t snap_type = pkt_read_u16_be(frame + 20);
    if(snap_type != CDP_SNAP_TYPE) return 0;

    /* CDP payload starts at offset 22 */
    return 22;
}

bool cdp_parse(const uint8_t* payload, uint16_t payload_len, CdpNeighbor* neighbor) {
    furi_assert(payload);
    furi_assert(neighbor);

    memset(neighbor, 0, sizeof(CdpNeighbor));

    /*
     * CDP header:
     *   1 byte: Version
     *   1 byte: TTL
     *   2 bytes: Checksum
     */
    if(payload_len < 4) return false;

    neighbor->cdp_version = payload[0];
    neighbor->cdp_ttl = payload[1];
    /* Skip checksum at offset 2-3 */

    uint16_t offset = 4;

    while(offset + 4 <= payload_len) {
        uint16_t tlv_type = pkt_read_u16_be(payload + offset);
        uint16_t tlv_len = pkt_read_u16_be(payload + offset + 2);

        /* TLV length includes the 4-byte header */
        if(tlv_len < 4 || offset + tlv_len > payload_len) {
            FURI_LOG_W(TAG, "Invalid CDP TLV (type=0x%04X, len=%d)", tlv_type, tlv_len);
            break;
        }

        const uint8_t* tlv_data = payload + offset + 4;
        uint16_t data_len = tlv_len - 4;

        switch(tlv_type) {
        case CDP_TLV_DEVICE_ID:
            if(data_len > 0) {
                uint16_t copy_len = (data_len < CDP_MAX_STRING - 1) ? data_len :
                                                                      CDP_MAX_STRING - 1;
                memcpy(neighbor->device_id, tlv_data, copy_len);
            }
            break;

        case CDP_TLV_ADDRESSES:
            /*
             * Addresses TLV:
             *   4 bytes: number of addresses
             *   For each address:
             *     1 byte: protocol type (1 = NLPID)
             *     1 byte: protocol length
             *     N bytes: protocol
             *     2 bytes: address length
             *     N bytes: address
             *
             * We only extract the first IPv4 address.
             */
            if(data_len >= 13) {
                /* Skip number_of_addresses (4 bytes) */
                /* Check first address: protocol_type=1 (NLPID), proto_len=1, proto=0xCC (IPv4) */
                uint8_t proto_type = tlv_data[4];
                uint8_t proto_len = tlv_data[5];
                if(proto_type == 1 && proto_len == 1 && tlv_data[6] == 0xCC) {
                    uint16_t addr_len = pkt_read_u16_be(tlv_data + 7);
                    if(addr_len == 4 && data_len >= 13) {
                        memcpy(neighbor->mgmt_ip, tlv_data + 9, 4);
                    }
                }
            }
            break;

        case CDP_TLV_PORT_ID:
            if(data_len > 0) {
                uint16_t copy_len = (data_len < CDP_MAX_STRING - 1) ? data_len :
                                                                      CDP_MAX_STRING - 1;
                memcpy(neighbor->port_id, tlv_data, copy_len);
            }
            break;

        case CDP_TLV_CAPABILITIES:
            if(data_len >= 4) {
                neighbor->capabilities = pkt_read_u32_be(tlv_data);
            }
            break;

        case CDP_TLV_SW_VERSION:
            if(data_len > 0) {
                uint16_t copy_len = (data_len < CDP_MAX_STRING - 1) ? data_len :
                                                                      CDP_MAX_STRING - 1;
                memcpy(neighbor->sw_version, tlv_data, copy_len);
            }
            break;

        case CDP_TLV_PLATFORM:
            if(data_len > 0) {
                uint16_t copy_len = (data_len < CDP_MAX_STRING - 1) ? data_len :
                                                                      CDP_MAX_STRING - 1;
                memcpy(neighbor->platform, tlv_data, copy_len);
            }
            break;

        case CDP_TLV_VTP_DOMAIN:
            if(data_len > 0) {
                uint16_t copy_len = (data_len < CDP_MAX_STRING - 1) ? data_len :
                                                                      CDP_MAX_STRING - 1;
                memcpy(neighbor->vtp_domain, tlv_data, copy_len);
            }
            break;

        case CDP_TLV_NATIVE_VLAN:
            if(data_len >= 2) {
                neighbor->native_vlan = pkt_read_u16_be(tlv_data);
            }
            break;

        case CDP_TLV_DUPLEX:
            if(data_len >= 1) {
                neighbor->duplex = tlv_data[0];
            }
            break;

        default:
            /* Unknown TLV, skip */
            break;
        }

        offset += tlv_len;
    }

    if(neighbor->device_id[0] != '\0') {
        neighbor->valid = true;
    }

    return neighbor->valid;
}

void cdp_format_neighbor(const CdpNeighbor* neighbor, char* buf, uint16_t buf_size) {
    furi_assert(neighbor);
    furi_assert(buf);

    char ip_str[16];
    pkt_format_ip(neighbor->mgmt_ip, ip_str);

    snprintf(
        buf,
        buf_size,
        "=== CDP Neighbor ===\n"
        "Device: %s\n"
        "Platform: %s\n"
        "Port: %s\n"
        "Mgmt IP: %s\n"
        "VLAN: %d\n"
        "Duplex: %s\n"
        "Version: %s\n"
        "VTP: %s\n"
        "Caps: 0x%08lX\n",
        neighbor->device_id[0] ? neighbor->device_id : "(none)",
        neighbor->platform[0] ? neighbor->platform : "(none)",
        neighbor->port_id[0] ? neighbor->port_id : "(none)",
        ip_str,
        neighbor->native_vlan,
        neighbor->duplex ? "Full" : "Half",
        neighbor->sw_version[0] ? neighbor->sw_version : "(none)",
        neighbor->vtp_domain[0] ? neighbor->vtp_domain : "(none)",
        (unsigned long)neighbor->capabilities);
}
