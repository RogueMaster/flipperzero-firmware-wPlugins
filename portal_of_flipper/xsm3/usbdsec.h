/*
    usbdsec.h - part of libxsm3
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

#ifndef USBDSEC_H_
#define USBDSEC_H_

#include <stddef.h>

void UsbdSecXSM3AuthenticationCrypt(const uint8_t *key, const uint8_t *input, size_t length, uint8_t *output, uint8_t encrypt);
void UsbdSecXSM3AuthenticationMac(const uint8_t *key, const uint8_t *salt, uint8_t *input, size_t length, uint8_t *output);
void UsbdSecXSMAuthenticationAcr(const uint8_t *console_id, const uint8_t *input, const uint8_t *key, uint8_t *output);

#endif // USBDSEC_H_