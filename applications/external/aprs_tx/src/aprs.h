#pragma once

#include "packet.h"

#include <stdint.h>
#include <stdbool.h>

int aprs_lat(char *out, uint16_t n, const char *s);
int aprs_lon(char *out, uint16_t n, const char *s);
int aprs_ll_clamp(char *out, uint16_t n, const char *s, uint8_t lon);

int aprs_bulletin(char *out, uint16_t n, uint8_t index, const char *text);
int aprs_status(char *out, uint16_t n, const char *text);
int aprs_message(char *out, uint16_t n, const char *dst, uint8_t ssid, const char *text);
int aprs_pos(char *out, uint16_t n, const char *name, const char *lat, const char *lon);

bool aprs_packet(Packet *p, const char *from, uint8_t from_ssid, const char *to, uint8_t to_ssid,
                 const char *payload, const char *path);
bool call_crc(const char *s, uint16_t ptr);

void packet_make_payload(Packet *p, const char *s);
void packet_make_ax25(Packet *p, const char *from, uint8_t from_ssid, const char *to,
                      uint8_t to_ssid, const char *path);
void packet_do_all(Packet *p, const char *from, uint8_t from_ssid, const char *to, uint8_t to_ssid,
                   const char *s, const char *path);
