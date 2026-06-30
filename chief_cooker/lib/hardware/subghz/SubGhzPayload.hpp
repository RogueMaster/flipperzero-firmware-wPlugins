#pragma once

#include <algorithm>
#include "flipper_format.h"

using namespace std;

#undef LOG_TAG
#define LOG_TAG "SG_PLD"

class SubGhzPayload {
private:
    const char* protocol;
    FlipperFormat* flipperFormat;
    int requiredSoftwareRepeats = 1;

public:
    SubGhzPayload(const char* protocol) {
        this->protocol = protocol;

        flipperFormat = flipper_format_string_alloc();
        flipper_format_write_string_cstr(flipperFormat, "Protocol", protocol);
    }

    void SetKey(uint64_t key) {
        char* dataBytes = (char*)&key;
        reverse(dataBytes, dataBytes + sizeof(key));
        flipper_format_write_hex(flipperFormat, "Key", (const uint8_t*)dataBytes, sizeof(key));
    }

    void SetBits(uint32_t bits) {
        flipper_format_write_uint32(flipperFormat, "Bit", &bits, 1);
    }

    void SetTE(uint32_t te) {
        flipper_format_write_uint32(flipperFormat, "TE", &te, 1);
    }

    void SetRepeat(uint32_t repeats) {
        flipper_format_write_uint32(flipperFormat, "Repeat", &repeats, 1);
    }

    void SetSoftwareRepeats(uint32_t repeats) {
        this->requiredSoftwareRepeats = repeats;
    }

    FlipperFormat* GetFlipperFormat() {
        return flipperFormat;
    }

    int GetRequiredSofwareRepeats() {
        return requiredSoftwareRepeats;
    }

    const char* GetProtocol() {
        return protocol;
    }

    ~SubGhzPayload() {
        if(flipperFormat != NULL) {
            flipper_format_free(flipperFormat);
            flipperFormat = NULL;
        }
    }
};
