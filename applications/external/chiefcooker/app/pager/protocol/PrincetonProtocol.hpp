#pragma once

#include <stream/stream.h>

#include "PagerProtocol.hpp"

class PrincetonProtocol : public PagerProtocol {
public:
    const char* GetSystemName() {
        return "Princeton";
    }

    int GetFallbackTE() {
        return 212;
    }

    int GetMaxTE() {
        return 1200;
    }

    SubGhzPayload* CreatePayload(uint64_t data, uint32_t te, uint32_t repeats) {
        SubGhzPayload* payload = new SubGhzPayload(GetSystemName());
        payload->SetBits(24);
        payload->SetKey(data);
        payload->SetTE(te);
        // somewhy repeats are always 10 even if we set it, so use here "software repeats" instead
        payload->SetSoftwareRepeats(ceil(repeats / 10.0));
        payload->SetRepeat(10); // just in case they'll fix it

        return payload;
    }
};
