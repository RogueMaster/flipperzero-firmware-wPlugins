#include "lldp.h"
#include "../utils/packet_utils.h"

#include <furi.h>
#include <string.h>

#define TAG "LLDP"

bool lldp_parse(const uint8_t* payload, uint16_t payload_len, LldpNeighbor* neighbor) {
    furi_assert(payload);
    furi_assert(neighbor);

    memset(neighbor, 0, sizeof(LldpNeighbor));

    uint16_t offset = 0;

    while(offset + 2 <= payload_len) {
        /* TLV header: 7 bits type + 9 bits length */
        uint16_t tlv_header = pkt_read_u16_be(payload + offset);
        uint8_t tlv_type = (tlv_header >> 9) & 0x7F;
        uint16_t tlv_len = tlv_header & 0x01FF;
        offset += 2;

        /* Bounds check */
        if(offset + tlv_len > payload_len) {
            FURI_LOG_W(TAG, "TLV length exceeds payload (type=%d, len=%d)", tlv_type, tlv_len);
            break;
        }

        const uint8_t* tlv_data = payload + offset;

        switch(tlv_type) {
        case LLDP_TLV_END:
            /* End of LLDPDU */
            neighbor->valid = true;
            return true;

        case LLDP_TLV_CHASSIS_ID:
            /* First byte is subtype */
            if(tlv_len >= 7 && tlv_data[0] == LLDP_CHASSIS_SUBTYPE_MAC) {
                memcpy(neighbor->chassis_mac, tlv_data + 1, 6);
            } else if(tlv_len > 1) {
                /* Store as string for non-MAC subtypes */
                uint16_t copy_len = (tlv_len - 1 < LLDP_MAX_STRING - 1) ? tlv_len - 1 :
                                                                          LLDP_MAX_STRING - 1;
                memcpy(neighbor->system_name, tlv_data + 1, copy_len);
            }
            break;

        case LLDP_TLV_PORT_ID:
            if(tlv_len > 1) {
                uint16_t copy_len = (tlv_len - 1 < LLDP_MAX_STRING - 1) ? tlv_len - 1 :
                                                                          LLDP_MAX_STRING - 1;
                memcpy(neighbor->port_id, tlv_data + 1, copy_len);
            }
            break;

        case LLDP_TLV_TTL:
            if(tlv_len >= 2) {
                neighbor->ttl = pkt_read_u16_be(tlv_data);
            }
            break;

        case LLDP_TLV_PORT_DESC:
            if(tlv_len > 0) {
                uint16_t copy_len = (tlv_len < LLDP_MAX_STRING - 1) ? tlv_len :
                                                                      LLDP_MAX_STRING - 1;
                memcpy(neighbor->port_desc, tlv_data, copy_len);
            }
            break;

        case LLDP_TLV_SYSTEM_NAME:
            if(tlv_len > 0) {
                uint16_t copy_len = (tlv_len < LLDP_MAX_STRING - 1) ? tlv_len :
                                                                      LLDP_MAX_STRING - 1;
                memcpy(neighbor->system_name, tlv_data, copy_len);
            }
            break;

        case LLDP_TLV_SYSTEM_DESC:
            if(tlv_len > 0) {
                uint16_t copy_len = (tlv_len < LLDP_MAX_STRING - 1) ? tlv_len :
                                                                      LLDP_MAX_STRING - 1;
                memcpy(neighbor->system_desc, tlv_data, copy_len);
            }
            break;

        case LLDP_TLV_SYSTEM_CAP:
            if(tlv_len >= 4) {
                neighbor->capabilities = pkt_read_u16_be(tlv_data);
                neighbor->enabled_capabilities = pkt_read_u16_be(tlv_data + 2);
            }
            break;

        case LLDP_TLV_MGMT_ADDR:
            /*
             * Management Address TLV:
             *   1 byte: addr string length (including subtype)
             *   1 byte: addr subtype (1 = IPv4)
             *   N bytes: address
             *   1 byte: interface numbering subtype
             *   4 bytes: interface number
             *   1 byte: OID string length
             *   N bytes: OID
             */
            if(tlv_len >= 7) {
                uint8_t addr_len = tlv_data[0];
                uint8_t addr_subtype = tlv_data[1];
                if(addr_subtype == 1 && addr_len >= 5) {
                    /* IPv4 */
                    memcpy(neighbor->mgmt_ip, tlv_data + 2, 4);
                }
            }
            break;

        case LLDP_TLV_ORG_SPECIFIC:
            /*
             * Org-specific TLV (Type 127):
             *   3 bytes: OUI
             *   1 byte: subtype
             *   N bytes: data
             *
             * IEEE 802.1 (OUI 00-80-C2):
             *   Subtype 3: VLAN Name
             *
             * IEEE 802.3 (OUI 00-12-0F):
             *   Subtype 1: MAC/PHY config
             */
            if(tlv_len >= 4) {
                /* Check for IEEE 802.1 VLAN (OUI 00-80-C2, subtype 3) */
                if(tlv_data[0] == 0x00 && tlv_data[1] == 0x80 && tlv_data[2] == 0xC2 &&
                   tlv_data[3] == 0x03) {
                    if(tlv_len >= 6) {
                        neighbor->mgmt_vlan = pkt_read_u16_be(tlv_data + 4);
                    }
                }
            }
            break;

        default:
            /* Unknown TLV type, skip */
            break;
        }

        offset += tlv_len;
    }

    /* If we reached the end without End TLV, still mark valid if we got data */
    if(neighbor->system_name[0] != '\0' || neighbor->chassis_mac[0] != 0) {
        neighbor->valid = true;
    }

    return neighbor->valid;
}

void lldp_format_neighbor(const LldpNeighbor* neighbor, char* buf, uint16_t buf_size) {
    furi_assert(neighbor);
    furi_assert(buf);

    char mac_str[18];
    char ip_str[16];

    pkt_format_mac(neighbor->chassis_mac, mac_str);
    pkt_format_ip(neighbor->mgmt_ip, ip_str);

    snprintf(
        buf,
        buf_size,
        "=== LLDP Neighbor ===\n"
        "Name: %s\n"
        "Port: %s\n"
        "Desc: %s\n"
        "Chassis: %s\n"
        "Mgmt IP: %s\n"
        "VLAN: %d\n"
        "TTL: %d sec\n"
        "Caps: 0x%04X\n"
        "Sys: %s\n",
        neighbor->system_name[0] ? neighbor->system_name : "(none)",
        neighbor->port_id[0] ? neighbor->port_id : "(none)",
        neighbor->port_desc[0] ? neighbor->port_desc : "(none)",
        mac_str,
        ip_str,
        neighbor->mgmt_vlan,
        neighbor->ttl,
        neighbor->capabilities,
        neighbor->system_desc[0] ? neighbor->system_desc : "(none)");
}
