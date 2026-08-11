/*
 * Host stand-in for the Flipper's serial-port identifiers. Only the enum is
 * needed: the fake UART ignores which port it was asked for.
 */
#pragma once

typedef enum {
    FuriHalSerialIdUsart, /* pins 13 (TX) / 14 (RX) */
    FuriHalSerialIdLpuart, /* pins 15 (TX) / 16 (RX) */

    FuriHalSerialIdMax,
} FuriHalSerialId;
