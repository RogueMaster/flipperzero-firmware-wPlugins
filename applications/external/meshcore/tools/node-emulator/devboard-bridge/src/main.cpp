/*
 * Transparent USB <-> UART bridge for the Flipper WiFi Devboard (ESP32-S2).
 *
 * A dumb pipe, deliberately. Bytes arriving on USB go out of Serial1 and back,
 * unchanged and unbuffered beyond what the hardware needs. All the protocol
 * lives on the laptop in meshcore_emulator.py; this board must not know that
 * MeshCore exists, because anything it "helps" with is something that can
 * disagree with a real node.
 *
 * Wiring, node side of the bridge -- nothing to wire if the board is seated on
 * the Flipper's header, which is what these pins are for:
 *
 *     Devboard TX (GPIO43) -> Flipper pin 14 (RX)
 *     Devboard RX (GPIO44) <- Flipper pin 13 (TX)
 *     GND                  <-> Flipper pin 11 or 18
 *
 * The GPIO numbers come from the devboard's own USB-UART bridge, which does
 * this same job (blackmagic-esp32-s2, main/usb-uart.c). They are deliberately
 * NOT the 17/18 that cli-commands-gpio.c labels CLI_TXD/CLI_RXD: that is UART1,
 * the board's own console, and it does not reach the Flipper header at all.
 * Writing to it fails silently -- the port opens, the writes succeed, and the
 * bytes are simply gone.
 *
 * 3.3 V logic on both sides, so no level shifting.
 */

#include <Arduino.h>

#ifndef BRIDGE_TX_PIN
#define BRIDGE_TX_PIN 17
#endif
#ifndef BRIDGE_RX_PIN
#define BRIDGE_RX_PIN 18
#endif
#ifndef BRIDGE_BAUD
#define BRIDGE_BAUD 115200
#endif

/* One MeshCore frame is at most 176 bytes; a chunk this size moves a whole
 * frame per iteration without ever holding one back. */
static const size_t kChunk = 256;

/* The default is 256 bytes, and that is not enough. A GET_CONTACTS reply is one
 * uninterrupted burst -- CONTACTS_START, a 151-byte frame per contact, then
 * END_OF_CONTACTS, so 604 bytes for four contacts and more for a real node.
 * While this loop is busy writing the other direction, anything past the buffer
 * is dropped by the UART, and the damage does not look like a lost frame: the
 * framer resyncs on the next plausible length and hands up a contact record
 * assembled from the middle of two others. A name arrived as "RO" instead of
 * "ROVER-M" that way, which is what led here.
 *
 * Sized for the largest burst a node with a full contact list can produce. */
static const size_t kRxBuffer = 4096;

void setup() {
    /* Both setRxBufferSize calls must come before their begin(). */
    Serial.setRxBufferSize(kRxBuffer);
    Serial.begin(BRIDGE_BAUD);

    Serial1.setRxBufferSize(kRxBuffer);
    Serial1.begin(BRIDGE_BAUD, SERIAL_8N1, BRIDGE_RX_PIN, BRIDGE_TX_PIN);

    /* No banner on Serial: this is a byte-exact pipe, and a greeting would land
     * in the middle of someone's protocol. */
}

void loop() {
    static uint8_t buffer[kChunk];

    int available = Serial.available();
    if(available > 0) {
        size_t n = Serial.readBytes(buffer, available > (int)kChunk ? kChunk : available);
        Serial1.write(buffer, n);
    }

    available = Serial1.available();
    if(available > 0) {
        size_t n = Serial1.readBytes(buffer, available > (int)kChunk ? kChunk : available);
        Serial.write(buffer, n);
    }
}
