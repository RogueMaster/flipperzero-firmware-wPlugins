#include "dhcp_protocol.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/protocol_tools/dhcp.h"
#include "../libraries/generals/ethernet_generals.h"

// F0.6 — HOST[] / host_size were used only by the deleted no-hostname
// DORA path (flipper_process_dora). The hostname-aware path takes the
// hostname as a parameter from the caller (app_worker.c passes "Flippa 0").

uint8_t MAC_ADDRESS[6] = {0, 0, 0, 0, 0, 0}; // Este lo vamos a cambiar desde el Flipper Zero

// The MAC destination for the DHCP SERVER at this case, the gateway or router/modem
uint8_t MAC_DESTINATION[6] = {0};

// Unique identifier for the DHCP communication
uint32_t xid = 0x0;

// This is for the IPv4 Header
uint8_t source_ip[] = {0x0, 0x0, 0x0, 0x0};
uint8_t destination_ip[] = {0xff, 0xff, 0xff, 0xff};

// The Gatway
uint8_t gateway[4] = {0};
uint8_t dns_server[4] = {0};
uint8_t subnet_mask[4] = {0};

// The dhcp_server
uint8_t dhcp_server_ip[4] = {0};

// Our IP
uint8_t myip[4] = {0};

// To put all the IP on zero
uint8_t ip_zeros[4] = {0};

// The IP client and IP server
uint8_t ip_client[4] = {0};
uint8_t ip_server[4] = {0};

// States process
typedef enum {
    DHCP_STATE_INIT,
    DHCP_STATE_REQUEST,
    DHCP_STATE_WAITING,
    DHCP_OK,
    DHCP_FAIL
} state_dora_t;

// F0.6 — set_dhcp_discover_message (no-hostname variant) deleted; the
// hostname-aware set_dhcp_discover_message_with_host_name is the only
// builder used in production.

// Function to get the dhcp message offer
bool deconstruct_dhcp_offer(uint8_t* buffer) {
    if(buffer == NULL) return false;

    if(!is_dhcp(buffer)) return false;

    dhcp_message_t dhcp_message = dhcp_deconstruct_dhcp_message(buffer);

    if(!dhcp_is_offer(dhcp_message)) return false;

    uint32_t is_the_xid = dhcp_message.xid[0] << 24 | dhcp_message.xid[1] << 16 |
                          dhcp_message.xid[2] << 8 | dhcp_message.xid[3];

    if(is_the_xid != xid) return false;

    // Get the MAC Adress from the dhcp server
    ethernet_header_t ethernet_header = ethernet_get_header(buffer);
    memcpy(MAC_DESTINATION, ethernet_header.mac_source, 6);

    // dhcp message yiaddr
    memcpy(ip_client, dhcp_message.yiaddr, 4);

    // dhcp message ip server
    uint8_t length = 0;
    dhcp_get_option_data(dhcp_message, DHCP_OP_SERVER_IDENTIFIER, ip_server, &length);

    return true;
}

// F0.6 — set_dhcp_request_message (no-hostname variant) deleted; the
// hostname-aware set_dhcp_request_message_with_host_name is used.

// Function to deconstruct the acknowledge message
bool deconstruct_dhcp_ack(uint8_t* buffer) {
    if(buffer == NULL) return false;

    if(!is_dhcp(buffer)) return false;

    dhcp_message_t dhcp_message = dhcp_deconstruct_dhcp_message(buffer);

    if(!dhcp_is_acknoledge(dhcp_message)) return false;

    uint32_t is_the_xid = dhcp_message.xid[0] << 24 | dhcp_message.xid[1] << 16 |
                          dhcp_message.xid[2] << 8 | dhcp_message.xid[3];

    if(is_the_xid != xid) return false;

    memcpy(myip, dhcp_message.yiaddr, 4);

    uint8_t length = 0;

    dhcp_get_option_data(dhcp_message, DHCP_OP_SUBNET_MASK, subnet_mask, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_ROUTER, gateway, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_NAME_SERVER, dns_server, &length);

    dhcp_get_option_data(dhcp_message, DHCP_OP_SERVER_IDENTIFIER, dhcp_server_ip, &length);

    return true;
}

// F0.6 — flipper_process_dora (no-hostname variant) deleted; only
// flipper_process_dora_with_host_name is used in production.

// Function to copy the MAC DESTINATION in this case the MAC of the router
void get_mac_server(uint8_t* MAC_SERVER) {
    memcpy(MAC_SERVER, MAC_DESTINATION, 6);
}

// Function to get the gateway
void get_gateway_ip(uint8_t* ip_gateway) {
    memcpy(ip_gateway, gateway, 4);
}

// Function to set a Discover Message
void set_dhcp_discover_message_with_host_name(uint8_t* buffer, uint16_t* length, const char* host) {
    if(buffer == NULL || length == NULL) return;

    uint16_t dhcp_len = 0;

    dhcp_message_t dhcp_message =
        dhcp_message_discover(MAC_ADDRESS, xid, (uint8_t*)host, &dhcp_len);

    set_ethernet_header(buffer, MAC_ADDRESS, MAC_BROADCAST, 0x800);

    set_ipv4_header(
        buffer + ETHERNET_HEADER_LEN,
        0x11,
        dhcp_len + UDP_HEADER_LEN,
        source_ip,
        destination_ip,
        0,
        0x4000,
        WIN_TTL);

    set_udp_header(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, 0x44, 0x43, dhcp_len + UDP_HEADER_LEN);

    memcpy(buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN, &dhcp_message, dhcp_len);

    *length = dhcp_len + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN;
}

// Function to set the dhcp message request
void set_dhcp_request_message_with_host_name(uint8_t* buffer, uint16_t* length, const char* host) {
    if(buffer == NULL || length == NULL) return;

    uint16_t dhcp_len = 0;

    dhcp_message_t dhcp_message =
        dhcp_message_request(MAC_ADDRESS, xid, ip_client, ip_server, (uint8_t*)host, &dhcp_len);

    set_ethernet_header(buffer, MAC_ADDRESS, MAC_BROADCAST, 0x800);

    set_ipv4_header(
        buffer + ETHERNET_HEADER_LEN,
        0x11,
        dhcp_len + UDP_HEADER_LEN,
        source_ip,
        destination_ip,
        0,
        0x4000,
        WIN_TTL);

    set_udp_header(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, 0x44, 0x43, dhcp_len + UDP_HEADER_LEN);

    memcpy(buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN, &dhcp_message, dhcp_len);

    *length = dhcp_len + ETHERNET_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN;
}

bool flipper_process_dora_with_host_name(
    enc28j60_t* ethernet,
    uint8_t* static_ip,
    uint8_t* ip_router,
    uint8_t* mac_router,
    const char* host,
    volatile const bool* cancel) {
    uint32_t current_time = furi_get_tick();

    bool ret = false;

    xid = furi_hal_random_get();

    set_mac_address(ethernet->mac_address);

    uint8_t* tx_buffer = ethernet->tx_buffer;
    uint8_t* rx_buffer = ethernet->rx_buffer;
    uint16_t length = 0;

    state_dora_t state = DHCP_STATE_INIT;

    enable_broadcast(ethernet);

    while(!ret && is_link_up(ethernet)) {
        // F0.5f — early-out on caller-requested cancel (e.g. user
        // pressed Back in GetIPScene). Without this, on_exit's
        // furi_thread_join would block up to the 10 s timeout below.
        if(cancel && *cancel) break;

        // F0.7 (B-1) — was 3000 ms, which is shorter than the time
        // a standards-compliant DHCP server takes to ARP-probe + ICMP-
        // probe a candidate IP before sending OFFER (≈ 2-3 s on
        // dnsmasq with default `--ping`). The Flipper used to give up
        // exactly when the OFFER arrived, retry DISCOVER 40 s later,
        // and the user saw a stuck "Waiting for IP". 10 s gives the
        // server room to do its checks and still feels reasonable.
        if(furi_get_tick() > (current_time + 10000)) {
            break;
        }

        switch(state) {
        // This state is to send the discover message
        case DHCP_STATE_INIT:
            set_dhcp_discover_message_with_host_name(tx_buffer, &length, host);
            send_packet(ethernet, tx_buffer, length);
            memset(tx_buffer, 0, MAX_FRAMELEN);
            current_time = furi_get_tick();
            state = DHCP_STATE_WAITING;
            break;

        // This state is to send the request message
        case DHCP_STATE_REQUEST:
            set_dhcp_request_message_with_host_name(tx_buffer, &length, host);
            send_packet(ethernet, tx_buffer, length);
            memset(tx_buffer, 0, MAX_FRAMELEN);
            current_time = furi_get_tick();
            state = DHCP_STATE_WAITING;
            break;

        // It will waiting for a dhcp message, any of offer or
        case DHCP_STATE_WAITING:
            length = receive_packet(ethernet, rx_buffer, MAX_FRAMELEN);

            // This part helps to know if it is dhcp offer
            if(is_dhcp(rx_buffer)) {
                if(deconstruct_dhcp_offer(rx_buffer)) {
                    state = DHCP_STATE_REQUEST; // set the state in request
                    memset(rx_buffer, 0, MAX_FRAMELEN);
                    current_time = furi_get_tick();
                }

                // This part helps to know if it is dhcp acknowledge
                if(deconstruct_dhcp_ack(rx_buffer)) {
                    // F0.7 — was `get_subnet_mask(mac_router)` which wrote
                    // the 4-byte subnet mask into the caller's 6-byte MAC
                    // buffer (so mac_router got 4 bytes of subnet + 2
                    // bytes of garbage and the chip's subnet_mask field
                    // never got populated). Now populates the chip's
                    // subnet_mask AND copies the resolved DHCP-server MAC
                    // (saved during deconstruct_dhcp_offer) into
                    // mac_router as advertised in the function contract.
                    get_subnet_mask(ethernet->subnet_mask);
                    memcpy(mac_router, MAC_DESTINATION, 6);
                    state = DHCP_OK; // state ok
                    ret = true;

                    current_time = furi_get_tick();
                }
            }
            break;

            // If the process fail it will stop and return a false
            // case DHCP_FAIL:
            //     return false;
            //     break;

        default:
            break;
        }

        furi_delay_us(1);
    }

    disable_broadcast(ethernet);

    if(ret) {
        memcpy(static_ip, myip, 4);

        memcpy(ip_router, gateway, 4);
    }

    return ret;
}

void set_mac_address(uint8_t* mac_address) {
    memcpy(MAC_ADDRESS, mac_address, 6);
}

void get_subnet_mask(uint8_t* mask) {
    memcpy(mask, subnet_mask, 4);
}
