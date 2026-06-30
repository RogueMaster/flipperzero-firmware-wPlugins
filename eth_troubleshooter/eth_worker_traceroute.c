#include "eth_worker.h"
#include <furi_hal.h>
#include <traceroute.h>

uint8_t traceroute_auto(uint8_t *adress)
{
    return traceroute(PING_SOCKET, adress);
}