#include "baud_table.h"

const HermesBaudEntry hermes_baud_table[HERMES_BAUD_COUNT] = {
    {1200, "legacy / meters"},
    {2400, ""},
    {4800, "GPS NMEA"},
    {9600, "the old default"},
    {14400, ""},
    {19200, ""},
    {28800, ""},
    {31250, "MIDI"},
    {38400, "GPS / modems"},
    {57600, ""},
    {74880, "ESP8266 boot log"},
    {115200, "the modern default"},
    {128000, ""},
    {230400, ""},
    {250000, "DMX512"},
    {460800, ""},
    {921600, "fast bootloaders"},
};

size_t hermes_baud_nearest_index(uint32_t baud) {
    size_t best = 0;
    uint32_t best_err = UINT32_MAX;

    for(size_t i = 0; i < HERMES_BAUD_COUNT; i++) {
        const uint32_t std = hermes_baud_table[i].baud;
        const uint32_t diff = (baud > std) ? (baud - std) : (std - baud);
        // Relative, not absolute: 300 baud off matters at 1200 and not at 921600.
        const uint32_t err = (uint32_t)(((uint64_t)diff * 1000000u) / std);
        if(err < best_err) {
            best_err = err;
            best = i;
        }
    }
    return best;
}

/* ------------------------------------------------------------- framing ---- */

const char* hermes_framing_name(HermesFraming framing) {
    switch(framing) {
    case HermesFraming8N1:
        return "8N1";
    case HermesFraming8E1:
        return "8E1";
    case HermesFraming8O1:
        return "8O1";
    case HermesFraming7E1:
        return "7E1";
    default:
        return "?";
    }
}

void hermes_framing_apply(FuriHalSerialHandle* handle, HermesFraming framing) {
    furi_assert(handle);

    /* Data bits here are payload only - the HAL adds the parity bit on top,
     * which is why 9 data bits are legal only with parity off. */
    switch(framing) {
    case HermesFraming8E1:
        furi_hal_serial_configure_framing(
            handle, FuriHalSerialDataBits8, FuriHalSerialParityEven, FuriHalSerialStopBits1);
        break;
    case HermesFraming8O1:
        furi_hal_serial_configure_framing(
            handle, FuriHalSerialDataBits8, FuriHalSerialParityOdd, FuriHalSerialStopBits1);
        break;
    case HermesFraming7E1:
        furi_hal_serial_configure_framing(
            handle, FuriHalSerialDataBits7, FuriHalSerialParityEven, FuriHalSerialStopBits1);
        break;
    case HermesFraming8N1:
    default:
        furi_hal_serial_configure_framing(
            handle, FuriHalSerialDataBits8, FuriHalSerialParityNone, FuriHalSerialStopBits1);
        break;
    }
}

/* ---------------------------------------------------------------- port ---- */

const char* hermes_port_name(HermesPort port) {
    return (port == HermesPortLpuart) ? "LPUART" : "USART";
}

const char* hermes_port_pins(HermesPort port) {
    return (port == HermesPortLpuart) ? "RX=16 TX=15" : "RX=14 TX=13";
}

FuriHalSerialId hermes_port_serial_id(HermesPort port) {
    return (port == HermesPortLpuart) ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
}

const GpioPin* hermes_port_rx_pin(HermesPort port) {
    return (port == HermesPortLpuart) ? &gpio_ext_pc0 : &gpio_usart_rx;
}
