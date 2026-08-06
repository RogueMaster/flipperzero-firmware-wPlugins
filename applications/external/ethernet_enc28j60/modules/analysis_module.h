#ifndef _ANALYSIS_MODULE_H_
#define _ANALYSIS_MODULE_H_

#include <furi.h>
#include <furi_hal.h>

/**
 * Get the text for the info of the packet
 */
void print_packet_info(FuriString* text, uint8_t* packet, uint16_t packet_size);

#endif
