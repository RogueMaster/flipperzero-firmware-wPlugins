/*
 * Link time trimming of esp-serial-flasher.
 *
 * A FAP is loaded into RAM and the Flipper only has 256 KB of it, so the two
 * parts of the library that are dead weight here are cut out:
 *
 *  - the upstream stub table references all eleven chip stubs, ~90 KB of
 *    blobs. Only the ESP32-S2 one can ever be used on this hardware.
 *  - protocol_spi.c / protocol_sdio.c are not compiled at all; esp_loader.c
 *    still references their ops getters from esp_loader_init_spi/sdio, which
 *    this app never calls.
 */

#include "esp_stubs.h"
#include "esp_loader_protocol.h"

extern const esp_stub_t esp_stub_esp32s2;

const esp_stub_t* const esp_stub[ESP_MAX_CHIP] = {
    [ESP32S2_CHIP] = &esp_stub_esp32s2,
};

/* Referenced by the ESP32-P4 revision check inside the library. Unreachable
 * here, but it has to resolve at link time. */
const esp_stub_t esp_stub_esp32p4rev1 = {0};

const esp_loader_protocol_ops_t* esp_loader_get_spi_ops(void) {
    return NULL;
}

const esp_loader_protocol_ops_t* esp_loader_get_sdio_ops(void) {
    return NULL;
}
