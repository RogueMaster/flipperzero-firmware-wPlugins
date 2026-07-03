#include "port_scan.h"

#include <furi.h>
#include <socket.h>
#include <wizchip_conf.h>

#define TAG "PORTSCAN"

/* Top-20 most common TCP ports */
const uint16_t PORT_PRESET_TOP20[] =
    {22, 23, 25, 53, 80, 110, 135, 139, 143, 443, 445, 993, 995, 3306, 3389, 5900, 8080, 8443};

/* Top-100 ports (most commonly open) */
const uint16_t PORT_PRESET_TOP100[] = {
    7,    9,     13,    21,    22,    23,    25,    26,    37,   53,   79,   80,   81,
    88,   106,   110,   111,   113,   119,   135,   139,   143,  144,  179,  199,  389,
    427,  443,   444,   445,   465,   513,   514,   515,   543,  544,  548,  554,  587,
    631,  646,   873,   990,   993,   995,   1025,  1026,  1027, 1028, 1029, 1110, 1433,
    1720, 1723,  1755,  1900,  2000,  2001,  2049,  2121,  2717, 3000, 3128, 3306, 3389,
    3986, 4899,  5000,  5009,  5051,  5060,  5101,  5190,  5357, 5432, 5631, 5666, 5800,
    5900, 6000,  6001,  6646,  7070,  8000,  8008,  8009,  8080, 8081, 8443, 8888, 9100,
    9999, 10000, 32768, 49152, 49153, 49154, 49155, 49156, 49157};

PortState port_scan_tcp(
    uint8_t socket_num,
    const uint8_t target_ip[4],
    uint16_t port,
    uint32_t timeout_ms) {
    /* Close any existing socket state */
    close(socket_num);

    /* Open TCP socket */
    int8_t ret = socket(socket_num, Sn_MR_TCP, 0, 0);
    if(ret != socket_num) {
        FURI_LOG_E(TAG, "Failed to open TCP socket: %d", ret);
        close(socket_num);
        return PortStateFiltered;
    }

    /* Attempt to connect */
    ret = connect(socket_num, (uint8_t*)target_ip, port);
    if(ret != SOCK_OK) {
        /* connect() returns SOCK_OK on success, otherwise error */
        /* On W5500, connect is non-blocking, we need to poll status */
    }

    /* Poll socket status until connected, closed, or timeout */
    uint32_t start_tick = furi_get_tick();
    PortState result = PortStateFiltered;

    while(furi_get_tick() - start_tick < timeout_ms) {
        uint8_t status = getSn_SR(socket_num);

        switch(status) {
        case SOCK_ESTABLISHED:
            /* Port is open! */
            result = PortStateOpen;
            goto done;

        case SOCK_CLOSE_WAIT:
            /* Connection was established then closed - still counts as open */
            result = PortStateOpen;
            goto done;

        case SOCK_CLOSED:
            /* Connection was refused (RST) */
            result = PortStateClosed;
            goto done;

        case SOCK_INIT:
        case SOCK_SYNSENT:
            /* Still connecting */
            break;

        default:
            break;
        }

        furi_delay_ms(10);
    }

done:
    /* Disconnect and close */
    disconnect(socket_num);
    close(socket_num);

    return result;
}
