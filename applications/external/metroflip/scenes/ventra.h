#pragma once

#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#define METROFLIP_VENTRA_MIN_PAGES 16

static inline bool metroflip_ventra_detect(const MfUltralightData* data) {
    return data && data->pages_read >= METROFLIP_VENTRA_MIN_PAGES &&
           data->page[4].data[0] == 0x0A && data->page[4].data[1] == 0x04 &&
           data->page[4].data[2] == 0x00 && data->page[6].data[0] == 0x00 &&
           data->page[6].data[1] == 0x00 && data->page[6].data[2] == 0x00;
}
