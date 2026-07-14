#ifndef TRACEROUTE_H
#define TRACEROUTE_H

#include "wizchip_conf.h"
#include "ping.h"
#include "socket.h"
#include "w5500.h"

#define MAX_HOPS 30
#define PORT 33434 // Keep the macro definition;
#define BUF_LEN 32

extern void ping_wait_ms(int ms);

uint8_t traceroute(uint8_t s, uint8_t *dest_addr);
void send_traceroute_request(uint8_t s, uint8_t *dest_addr, uint8_t ttl);
void receive_traceroute_reply(uint8_t s, uint8_t *addr, uint16_t len);
void set_ttl(uint8_t s, uint8_t ttl);
// uint16_t checksum(uint8_t *data_buf, uint16_t len);

#endif // TRACEROUTE_H
