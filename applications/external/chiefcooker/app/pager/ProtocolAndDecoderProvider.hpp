#pragma once

#include "protocol/PagerProtocol.hpp"
#include "decoder/PagerDecoder.hpp"

class ProtocolAndDecoderProvider {
public:
    virtual PagerProtocol* GetProtocolByName(const char* name) = 0;
    virtual PagerDecoder* GetDecoderByName(const char* name) = 0;
    virtual ~ProtocolAndDecoderProvider() {
    }
};
