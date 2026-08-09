#include "udp.h"
#include "dhcp.h"

dhcp_message_t
    dhcp_message_discover(uint8_t* MAC_ADDRESS, uint32_t xid, uint8_t* host_name, uint16_t* len) {
    dhcp_message_t message = {0};
    message.operation = 1;
    message.htype = 1;
    message.hlen = 6;
    message.hops = 0;

    message.xid[0] = (xid >> 24) & 0xff;
    message.xid[1] = (xid >> 16) & 0xff;
    message.xid[2] = (xid >> 8) & 0xff;
    message.xid[3] = (xid) & 0xff;

    memset(message.secs, 0, 2);
    memset(message.flags, 0, 2);

    memset(message.ciaddr, 0, 4);
    memset(message.yiaddr, 0, 4);
    memset(message.siaddr, 0, 4);
    memset(message.giaddr, 0, 4);

    memcpy(message.chaddr, MAC_ADDRESS, 6);

    memset(message.sname, 0, 64);
    memset(message.file, 0, 128);

    message.magic_cookie[0] = 0x63;
    message.magic_cookie[1] = 0x82;
    message.magic_cookie[2] = 0x53;
    message.magic_cookie[3] = 0x63;

    uint16_t size = 0;

    // This is the array with the values to send the request
    uint8_t first_option[] = {DHCP_OP_DHCP_MESSAGE_TYPE, 0x01, DHCP_DISCOVER};
    memcpy(message.dhcp_options, first_option, sizeof(first_option));
    size = sizeof(first_option);

    // This is to send the cliend identifier
    uint8_t second_option[] = {DHCP_OP_CLIENT_IDENTIFIER, 7, 0x1, 0, 0, 0, 0, 0, 0};
    memcpy(second_option + 3, MAC_ADDRESS, 6);

    memcpy(message.dhcp_options + size, second_option, sizeof(second_option));
    size += sizeof(second_option);

    // The host name
    uint8_t third_option[] = {DHCP_OP_HOST_NAME, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(third_option + 2, host_name, 8);

    memcpy(message.dhcp_options + size, third_option, sizeof(third_option));
    size += sizeof(third_option);

    // The requested list
    uint8_t fourth_option[] = {
        DHCP_OP_PARAMETER_REQUEST_LIST,
        3,
        DHCP_OP_SUBNET_MASK,
        DHCP_OP_ROUTER,
        DHCP_OP_DOMAIN_NAME_SERVER};

    memcpy(message.dhcp_options + size, fourth_option, sizeof(fourth_option));
    size += sizeof(fourth_option);

    message.dhcp_options[size++] = DHCP_END;

    *len = sizeof(dhcp_message_t) - 1000;
    *len += size;

    return message;
}

dhcp_message_t dhcp_message_request(
    uint8_t* MAC_ADDRESS,
    uint32_t xid,
    uint8_t* ip_client,
    uint8_t* ip_server,
    uint8_t* host_name,
    uint16_t* len) {
    dhcp_message_t message = {0};
    message.operation = 1;
    message.htype = 1;
    message.hlen = 6;
    message.hops = 0;

    message.xid[0] = (xid >> 24) & 0xff;
    message.xid[1] = (xid >> 16) & 0xff;
    message.xid[2] = (xid >> 8) & 0xff;
    message.xid[3] = (xid) & 0xff;

    memset(message.secs, 0, 2);
    memset(message.flags, 0, 2);

    memset(message.ciaddr, 0, 4);

    memset(message.siaddr, 0, 4);

    memcpy(message.chaddr, MAC_ADDRESS, 6);

    message.magic_cookie[0] = 0x63;
    message.magic_cookie[1] = 0x82;
    message.magic_cookie[2] = 0x53;
    message.magic_cookie[3] = 0x63;

    uint16_t size = 0;

    // This is the array with the values to send the request
    uint8_t first_option[] = {DHCP_OP_DHCP_MESSAGE_TYPE, 0x01, DHCP_REQUEST};
    memcpy(message.dhcp_options, first_option, sizeof(first_option));
    size = sizeof(first_option);

    // This is to send the cliend identifier
    uint8_t second_option[9] = {DHCP_OP_CLIENT_IDENTIFIER, 7, 0x1, 0, 0, 0, 0, 0, 0};
    memcpy(second_option + 3, MAC_ADDRESS, 6);

    memcpy(message.dhcp_options + size, second_option, sizeof(second_option));
    size += sizeof(second_option);

    // The host name
    uint8_t third_option[] = {DHCP_OP_HOST_NAME, 8, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(third_option + 2, host_name, 8);

    memcpy(message.dhcp_options + size, third_option, sizeof(third_option));
    size += sizeof(third_option);

    // Add the IP address
    uint8_t fourth_option[] = {DHCP_OP_REQUESTED_IP, 4, 0, 0, 0, 0};
    memcpy(fourth_option + 2, ip_client, 4);

    memcpy(message.dhcp_options + size, fourth_option, sizeof(fourth_option));
    size += sizeof(fourth_option);

    // Add the option of DHCP Server Identifier
    uint8_t fifth_option[] = {DHCP_OP_SERVER_IDENTIFIER, 4, 0, 0, 0, 0};
    memcpy(fifth_option + 2, ip_server, 4);

    memcpy(message.dhcp_options + size, fifth_option, sizeof(fifth_option));
    size += sizeof(fifth_option);

    // The requested list
    uint8_t sixth_option[] = {
        DHCP_OP_PARAMETER_REQUEST_LIST,
        3,
        DHCP_OP_SUBNET_MASK,
        DHCP_OP_ROUTER,
        DHCP_OP_DOMAIN_NAME_SERVER};

    memcpy(message.dhcp_options + size, sixth_option, sizeof(sixth_option));
    size += sizeof(sixth_option);

    message.dhcp_options[size++] = DHCP_END;

    *len = sizeof(dhcp_message_t) - 1000;
    *len += size;

    return message;
}

dhcp_message_t dhcp_deconstruct_dhcp_message(uint8_t* buffer) {
    udp_header_t udp_header = udp_get_header(buffer);
    uint16_t len = (udp_header.length[0] << 8 | udp_header.length[1]) - 8;

    dhcp_message_t message = {0};
    memcpy((uint8_t*)&message, buffer + 42, len);

    return message;
}

bool dhcp_get_option_data(dhcp_message_t message, uint8_t option, uint8_t* data, uint8_t* len_data) {
    uint8_t pos = 0;

    while(pos != 0xff) {
        if(message.dhcp_options[pos] == option) {
            memcpy(data, message.dhcp_options + pos + 2, message.dhcp_options[pos + 1]);
            *len_data = message.dhcp_options[pos + 1];

            return true;
        }
        pos = pos + message.dhcp_options[pos + 1] + 2;
    }

    return false;
}

bool dhcp_is_discover(dhcp_message_t message) {
    if(message.operation != 1) return false;
    if(message.dhcp_options[2] != DHCP_DISCOVER) return false;
    return true;
}

bool dhcp_is_offer(dhcp_message_t message) {
    if(message.operation != 2) return false;
    if(message.dhcp_options[2] != DHCP_OFFER) return false;
    return true;
}

bool dhcp_is_request(dhcp_message_t message) {
    if(message.operation != 1) return false;
    if(message.dhcp_options[2] != DHCP_REQUEST) return false;
    return true;
}

bool dhcp_is_acknoledge(dhcp_message_t message) {
    if(message.operation != 2) return false;
    if(message.dhcp_options[2] != DHCP_ACKNOLEDGE) return false;

    return true;
}

bool is_dhcp(uint8_t* buffer) {
    if(!is_udp(buffer)) return false;

    udp_header_t udp_header = udp_get_header(buffer);

    uint16_t source_port = udp_header.source_port[0] << 8 | udp_header.source_port[1];
    uint16_t dest_port = udp_header.dest_port[0] << 8 | udp_header.dest_port[1];

    if(source_port == 0x44 || source_port == 0x43) return true;
    if(dest_port == 0x44 || dest_port == 0x43) return true;

    return false;
}
