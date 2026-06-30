/*
    xsm3.h - part of libxsm3
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

#ifndef XSM3_H_
#define XSM3_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Identification data taken from an official wired controller. (Serial number is static.)
extern uint8_t xsm3_id_data_ms_controller[0x1D];

// The response data from the previously completed challenge.
extern uint8_t xsm3_challenge_response[0x30];

// The console ID fetched from the console after request 0x82.
extern uint8_t xsm3_console_id[0x8];

// Clears the state of the XSM3 internal variables.
void xsm3_initialise_state();

// Sets the identification data to use.
void xsm3_set_identification_data(const uint8_t id_data[0x1D]);

// Initialises the XSM3 state using information from the challenge init packet (0x82) and places a response in xsm3_challenge_response.
void xsm3_do_challenge_init(uint8_t challenge_packet[0x22]);

// Completes a verify challenge passed from request 0x87 and places the response data in xsm3_challenge_response.
void xsm3_do_challenge_verify(uint8_t challenge_packet[0x16]);

void xsm3_set_vid_pid(const uint8_t serial[0x0C], uint16_t vid, uint16_t pid);
#ifdef __cplusplus
}
#endif
#endif // XSM3_H_