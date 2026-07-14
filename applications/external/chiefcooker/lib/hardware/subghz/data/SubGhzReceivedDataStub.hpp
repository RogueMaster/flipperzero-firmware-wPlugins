#pragma once

#include <lib/subghz/protocols/base.h>

#include "SubGhzReceivedData.hpp"

class SubGhzReceivedDataStub : public SubGhzReceivedData {
private:
    const char* protocolName;
    uint32_t frequency;
    uint32_t hash;
    int te;

public:
    SubGhzReceivedDataStub(const char* protocolName, uint32_t hash) : SubGhzReceivedDataStub(protocolName, 433920000, hash, 212) {
    }

    SubGhzReceivedDataStub(const char* protocolName, uint32_t frequency, uint32_t hash, int te) {
        this->protocolName = protocolName;
        this->frequency = frequency;
        this->hash = hash;
        this->te = te;
    }

    const char* GetProtocolName() {
        return protocolName;
    }

    uint32_t GetHash() {
        return hash;
    }

    int GetTE() {
        return te;
    }

    uint32_t GetFrequency() {
        return frequency;
    }
};
