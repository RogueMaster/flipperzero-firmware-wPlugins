#ifndef UDP_MODULE_H_
#define UDP_MODULE_H_

// Note: This module will be in development
#include <furi.h>

void udp_port_scan(void* context, uint8_t* target_ip, uint16_t init_port, uint16_t range_port);

#endif
