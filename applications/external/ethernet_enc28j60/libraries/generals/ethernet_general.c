#include "ethernet_generals.h"

uint8_t MAC_ZEROS[6] = {0}; // MAC address in zeros
uint8_t MAC_BROADCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // MAC address for broadcast
uint8_t MAC_SPOOF[6] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01}; // MAC address for spoof

uint8_t IP_ZEROS[4] = {0}; // ip address in zeros
uint8_t IP_BROADCAST[4] = {0xff, 0xff, 0xff, 0xff}; // ip address for broadcast
