#pragma once

#include <lib/subghz/protocols/base.h>

#include "SubGhzReceivedData.hpp"

class SubGhzReceivedDataImpl : public SubGhzReceivedData {
private:
    uint32_t frequency;
    SubGhzProtocolDecoderBase* decoder;

public:
    SubGhzReceivedDataImpl(SubGhzProtocolDecoderBase* decoder, uint32_t frequency) {
        this->frequency = frequency;
        this->decoder = decoder;
    }

    const char* GetProtocolName() {
        return decoder->protocol->name;
    }

    uint32_t GetHash() {
        return decoder->protocol->decoder->get_hash_data_long(decoder);
    }

    int GetTE() {
        FuriString* dataString = furi_string_alloc();
        decoder->protocol->decoder->get_string(decoder, dataString);

        const char* tePrefix = "Te:";
        size_t teStart = furi_string_search_str(dataString, tePrefix, 0);
        if(teStart == FURI_STRING_FAILURE) {
            return -1;
        }

        const char* cstr = furi_string_get_cstr(dataString);
        const char* startPtr = cstr + teStart + strlen(tePrefix);
        int te = strtol(startPtr, NULL, 10);
        furi_string_free(dataString);

        return te;
    }

    uint32_t GetFrequency() {
        return frequency;
    }
};
