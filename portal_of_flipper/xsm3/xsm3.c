/*
    xsm3.c - part of libxsm3
    Copyright (C) 2022 InvoxiPlayGames

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "xsm3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "furi_hal_random.h"

#include "excrypt.h"
#include "usbdsec.h"

// disable debugging by specifying XSM3_NO_DEBUGGING at compile time
#ifndef XSM3_NO_DEBUGGING
#define XSM3_printf printf
#else
#define XSM3_printf
#endif  // XSM3_NO_DEBUGGING

// constant variables
uint8_t xsm3_id_data_ms_controller[0x1D] = {
    0x49, 0x4B, 0x00, 0x00, 0x17, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x00, 0x00, 0x80, 0x02, 0x09, 0x12, 0x82, 0x28,
    0x03, 0x00, 0x01, 0x01, 0x71};

// static global keys from the keyvault (shared across every retail system)
static const uint8_t xsm3_key_0x1D[0x10] = {
    0xE3, 0x5B, 0xFB, 0x1C, 0xCD, 0xAD, 0x32, 0x5B,
    0xF7, 0x0E, 0x07, 0xFD, 0x62, 0x3D, 0xA7, 0xC4};
static const uint8_t xsm3_key_0x1E[0x10] = {
    0x8F, 0x29, 0x08, 0x38, 0x0B, 0x5B, 0xFE, 0x68,
    0x7C, 0x26, 0x46, 0x2A, 0x51, 0xF2, 0xBC, 0x19};

// retail keys for generating 0x23/0x24 keys from console ID
static const uint8_t xsm3_root_key_0x23[0x10] = {
    0x82, 0x80, 0x78, 0x68, 0x3A, 0x52, 0x3A, 0x98,
    0x10, 0xF4, 0x0C, 0x12, 0x70, 0x66, 0xDC, 0xBA};

static const uint8_t xsm3_root_key_0x24[0x10] = {
    0x66, 0x62, 0x1A, 0x78, 0xF8, 0x60, 0x9C, 0x8A,
    0x26, 0x9A, 0x04, 0xAE, 0xD8, 0x5C, 0x1E, 0xC8};

// response to give to the given challenge command
uint8_t xsm3_challenge_response[0x30];
// console id fetched from challenge init packet
uint8_t xsm3_console_id[0x8];

// buffer to store the first key fetched from the KV (0x23)
static uint8_t xsm3_kv_2des_key_1[0x10];
// buffer to store the second key fetched from the KV (0x24)
static uint8_t xsm3_kv_2des_key_2[0x10];

// temporary buffer for decrypting packets
static uint8_t xsm3_decryption_buffer[0x30];
// identification data for the current device
static uint8_t xsm3_identification_data[0x20];
// random data sent from the console during the challenge init stage
static uint8_t xsm3_random_console_data[0x10];
static uint8_t xsm3_random_console_data_enc[0x10];
static uint8_t xsm3_random_console_data_swap[0x10];
static uint8_t xsm3_random_console_data_swap_enc[0x10];
// random data set by the controller during the challenge init stage
static uint8_t xsm3_random_controller_data[0x10];
// hash of the decrypted data sent by the controller during challenge init
static uint8_t xsm3_challenge_init_hash[0x14];
void xsm3_initialise_state() {
    // set all variables to all zeroes
    memset(xsm3_challenge_response, 0, sizeof(xsm3_challenge_response));
    memset(xsm3_console_id, 0, sizeof(xsm3_console_id));
    memset(xsm3_kv_2des_key_1, 0, sizeof(xsm3_kv_2des_key_1));
    memset(xsm3_kv_2des_key_2, 0, sizeof(xsm3_kv_2des_key_2));
    memset(xsm3_decryption_buffer, 0, sizeof(xsm3_decryption_buffer));
    memset(xsm3_identification_data, 0, sizeof(xsm3_identification_data));
    memset(xsm3_random_console_data, 0, sizeof(xsm3_random_console_data));
    memset(xsm3_random_console_data_enc, 0, sizeof(xsm3_random_console_data_enc));
    memset(xsm3_random_console_data_swap, 0, sizeof(xsm3_random_console_data_swap));
    memset(xsm3_random_console_data_swap_enc, 0, sizeof(xsm3_random_console_data_swap_enc));
    memset(xsm3_random_controller_data, 0, sizeof(xsm3_random_controller_data));
    memset(xsm3_challenge_init_hash, 0, sizeof(xsm3_challenge_init_hash));
}

static uint8_t xsm3_calculate_checksum(const uint8_t* packet) {
    // packet length in header doesn't include the header itself
    uint8_t packet_length = packet[0x4] + 0x5;
    uint8_t checksum = 0x00;
    int i = 0;
    // checksum is just a XOR over all packet bytes
    for (i = 0x5; i < packet_length; i++) {
        checksum ^= packet[i];
    }
    // last byte of the packet is the checksum
    return checksum;
}

void xsm3_set_vid_pid(const uint8_t serial[0x0C], uint16_t vid, uint16_t pid) {
    memcpy(xsm3_id_data_ms_controller + 6, serial, 0x0C);
    uint8_t* id_data = xsm3_id_data_ms_controller;
    // skip over the packet header
    id_data += 0x5;
    // vendor ID
    memcpy(id_data + 0xf, &vid, sizeof(unsigned short));
    // product ID
    memcpy(id_data + 0x11, &pid, sizeof(unsigned short));
    xsm3_id_data_ms_controller[0x1C] = xsm3_calculate_checksum(xsm3_id_data_ms_controller);
}

static bool xsm3_verify_checksum(const uint8_t* packet) {
    // packet length in header doesn't include the header itself
    uint8_t packet_length = packet[0x4] + 0x5;
    // last byte of the packet is the checksum
    return (xsm3_calculate_checksum(packet) == packet[packet_length]);
}

void xsm3_set_identification_data(const uint8_t id_data[0x1D]) {
    // validate the checksum
    if (!xsm3_verify_checksum(id_data)) {
        XSM3_printf("[ Checksum failed when setting identification data! ]\n");
    }

    // skip over the packet header
    id_data += 0x5;

    // prepare the xsm3_identification_data buffer

    // contains serial number (len: 0xC), unknown (len: 0x2) and the "category node" to use (len: 0x1)
    memcpy(xsm3_identification_data, id_data, 0xF);
    // vendor ID
    memcpy(xsm3_identification_data + 0x10, id_data + 0xF, sizeof(unsigned short));
    // product ID
    memcpy(xsm3_identification_data + 0x12, id_data + 0x11, sizeof(unsigned short));
    // unknown
    memcpy(xsm3_identification_data + 0x14, id_data + 0x13, sizeof(unsigned char));
    // unknown
    memcpy(xsm3_identification_data + 0x15, id_data + 0x16, sizeof(unsigned char));
    // unknown
    memcpy(xsm3_identification_data + 0x16, id_data + 0x14, sizeof(unsigned short));
}

void xsm3_generate_kv_keys(const uint8_t console_id[0x8]) {
    // make a sha-1 hash of the console id
    uint8_t console_id_hash[0x14];
    ExCryptSha(console_id, 0x8, NULL, 0, NULL, 0, console_id_hash, 0x14);
    // encrypt it with the root keys for 1st party controllers
    UsbdSecXSM3AuthenticationCrypt(xsm3_root_key_0x23, console_id_hash, 0x10, xsm3_kv_2des_key_1, 1);
    UsbdSecXSM3AuthenticationCrypt(xsm3_root_key_0x24, console_id_hash + 0x4, 0x10, xsm3_kv_2des_key_2, 1);
}

void xsm3_do_challenge_init(uint8_t challenge_packet[0x22]) {
    uint8_t incoming_packet_mac[0x8];
    uint8_t response_packet_mac[0x8];
    int i = 0;

    // validate the checksum
    if (!xsm3_verify_checksum(challenge_packet)) {
        XSM3_printf("[ Checksum failed when validating challenge init! ]\n");
    }

    // decrypt the packet content using the static key from the keyvault
    UsbdSecXSM3AuthenticationCrypt(xsm3_key_0x1D, challenge_packet + 0x5, 0x18, xsm3_decryption_buffer, 0);
    // first 0x10 bytes are random data
    memcpy(xsm3_random_console_data, xsm3_decryption_buffer, 0x10);
    // next 0x8 bytes are from the console certificate
    memcpy(xsm3_console_id, xsm3_decryption_buffer + 0x10, 0x8);
    // last 4 bytes of the packet are the last 4 bytes of the MAC
    UsbdSecXSM3AuthenticationMac(xsm3_key_0x1E, NULL, challenge_packet + 5, 0x18, incoming_packet_mac);
    // validate the MAC
    if (memcmp(incoming_packet_mac + 4, challenge_packet + 0x5 + 0x18, 0x4) != 0) {
        XSM3_printf("[ MAC failed when validating challenge init! ]\n");
    }
    xsm3_generate_kv_keys(xsm3_console_id);

    // the random value is swapped at an 8 byte boundary
    memcpy(xsm3_random_console_data_swap, xsm3_random_console_data + 0x8, 0x8);
    memcpy(xsm3_random_console_data_swap + 0x8, xsm3_random_console_data, 0x8);
    // and then encrypted - the regular value encrypted with key 1, the swapped value encrypted with key 2
    UsbdSecXSM3AuthenticationCrypt(xsm3_kv_2des_key_1, xsm3_random_console_data, 0x10, xsm3_random_console_data_enc, 1);
    UsbdSecXSM3AuthenticationCrypt(xsm3_kv_2des_key_2, xsm3_random_console_data_swap, 0x10, xsm3_random_console_data_swap_enc, 1);

    // generate random data
    for (i = 0; i < 0x10; i++) {
        xsm3_random_controller_data[i] = furi_hal_random_get() & 0xFF;
    }

    // clear response buffers
    memset(xsm3_challenge_response, 0, sizeof(xsm3_challenge_response));
    memset(xsm3_decryption_buffer, 0, sizeof(xsm3_decryption_buffer));
    // set header and packet length of challenge response
    xsm3_challenge_response[0] = 0x49;  // packet magic
    xsm3_challenge_response[1] = 0x4C;
    xsm3_challenge_response[4] = 0x28;  // packet length
    // copy random controller, random console data to the encryption buffer
    memcpy(xsm3_decryption_buffer, xsm3_random_controller_data, 0x10);
    memcpy(xsm3_decryption_buffer + 0x10, xsm3_random_console_data, 0x10);
    // save the sha1 hash of the decrypted contents for later
    ExCryptSha(xsm3_decryption_buffer, 0x20, NULL, 0, NULL, 0, xsm3_challenge_init_hash, 0x14);

    // encrypt challenge response packet using the encrypted random key
    UsbdSecXSM3AuthenticationCrypt(xsm3_random_console_data_enc, xsm3_decryption_buffer, 0x20, xsm3_challenge_response + 0x5, 1);
    // calculate MAC using the encrypted swapped random key and use it to calculate ACR
    UsbdSecXSM3AuthenticationMac(xsm3_random_console_data_swap_enc, NULL, xsm3_challenge_response + 0x5, 0x20, response_packet_mac);
    // calculate ACR and append to the end of the xsm3_challenge_response
    UsbdSecXSMAuthenticationAcr(xsm3_console_id, xsm3_identification_data, response_packet_mac, xsm3_challenge_response + 0x5 + 0x20);
    // calculate the checksum for the response packet
    xsm3_challenge_response[0x5 + 0x28] = xsm3_calculate_checksum(xsm3_challenge_response);

    // the console random value changes slightly after this point
    memcpy(xsm3_random_console_data, xsm3_random_controller_data + 0xC, 0x4);
    memcpy(xsm3_random_console_data + 0x4, xsm3_random_console_data + 0xC, 0x4);
}

void xsm3_do_challenge_verify(uint8_t challenge_packet[0x16]) {
    uint8_t incoming_packet_mac[0x8];

    // validate the checksum
    if (!xsm3_verify_checksum(challenge_packet)) {
        XSM3_printf("[ Checksum failed when validating challenge verify! ]\n");
    }

    // decrypt the packet using the controller generated random value
    UsbdSecXSM3AuthenticationCrypt(xsm3_random_controller_data, challenge_packet + 0x5, 0x8, xsm3_decryption_buffer, 0);
    // replace part of our random encryption value with the decrypted buffer
    memcpy(xsm3_random_console_data + 0x8, xsm3_decryption_buffer, 0x8);

    // calculate the MAC of the incoming packet
    UsbdSecXSM3AuthenticationMac(xsm3_challenge_init_hash, xsm3_random_console_data, challenge_packet + 0x5, 0x8, incoming_packet_mac);
    // validate the MAC
    if (memcmp(incoming_packet_mac, challenge_packet + 0x5 + 0x8, 0x8) != 0) {
        XSM3_printf("[ MAC failed when validating challenge verify! ]\n");
    }
    // clear response buffers
    memset(xsm3_challenge_response, 0, sizeof(xsm3_challenge_response));
    memset(xsm3_decryption_buffer, 0, sizeof(xsm3_decryption_buffer));
    // set header and packet length of challenge response
    xsm3_challenge_response[0] = 0x49;  // packet magic
    xsm3_challenge_response[1] = 0x4C;
    xsm3_challenge_response[4] = 0x10;  // packet length
    // calculate the ACR value and encrypt it into the outgoing packet using the encrypted random
    UsbdSecXSMAuthenticationAcr(xsm3_console_id, xsm3_identification_data, xsm3_random_console_data + 0x8, xsm3_decryption_buffer);
    UsbdSecXSM3AuthenticationCrypt(xsm3_random_console_data_enc, xsm3_decryption_buffer, 0x8, xsm3_challenge_response + 0x5, 1);
    // calculate the MAC of the encrypted packet and append it to the end
    UsbdSecXSM3AuthenticationMac(xsm3_random_console_data_swap_enc, xsm3_random_console_data, xsm3_challenge_response + 0x5, 0x8, xsm3_challenge_response + 0x5 + 0x8);
    // calculate the checksum for the response packet
    xsm3_challenge_response[0x5 + 0x10] = xsm3_calculate_checksum(xsm3_challenge_response);
}