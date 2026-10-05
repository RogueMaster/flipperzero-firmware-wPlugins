/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_access.h"

#include <string.h>

#include "../protocol/emrtd_mrz.h"

/*
 * Preference order, which is what EmrtdAccessMethodAuto walks when no driver
 * scores higher than another. PACE comes first because a document issued after
 * 2017 may implement it alone, while a document that still speaks BAC has
 * almost always kept it; trying the other way round would fail on the newer
 * documents and succeed slowly on the older ones.
 */
static const EmrtdAccessDriver* const emrtd_access_drivers[] = {
    &emrtd_access_driver_pace,
    &emrtd_access_driver_bac,
};

static bool emrtd_access_is_date(const char* value) {
    if(strlen(value) != EMRTD_DATE_LEN) {
        return false;
    }
    for(size_t i = 0; i < EMRTD_DATE_LEN; i++) {
        if(value[i] < '0' || value[i] > '9') {
            return false;
        }
    }
    return true;
}

bool emrtd_credentials_valid_mrz(const EmrtdCredentials* credentials) {
    if(credentials == NULL) {
        return false;
    }

    const size_t number_len = strlen(credentials->document_number);
    if(number_len == 0 || number_len > EMRTD_DOC_NUMBER_MAX) {
        return false;
    }
    for(size_t i = 0; i < number_len; i++) {
        /*
         * The key derivation runs the document number through the MRZ check
         * digit arithmetic, which only knows A to Z, 0 to 9 and the filler.
         * Anything else would derive a key from a character the chip never
         * saw, and the failure would look like a wrong passport.
         */
        if(emrtd_mrz_char_value(credentials->document_number[i]) < 0) {
            return false;
        }
    }

    return emrtd_access_is_date(credentials->date_of_birth) &&
           emrtd_access_is_date(credentials->date_of_expiry);
}

const char* emrtd_access_method_name(EmrtdAccessMethod method) {
    switch(method) {
    case EmrtdAccessMethodAuto:
        return "Automatic";
    case EmrtdAccessMethodPace:
        return "PACE";
    case EmrtdAccessMethodBac:
        return "BAC";
    default:
        return "?";
    }
}

const EmrtdAccessDriver* emrtd_access_driver_at(size_t index) {
    if(index >= emrtd_access_driver_count()) {
        return NULL;
    }
    return emrtd_access_drivers[index];
}

size_t emrtd_access_driver_count(void) {
    return sizeof(emrtd_access_drivers) / sizeof(emrtd_access_drivers[0]);
}

const EmrtdAccessDriver* emrtd_access_driver_for_method(EmrtdAccessMethod method) {
    switch(method) {
    case EmrtdAccessMethodPace:
        return &emrtd_access_driver_pace;
    case EmrtdAccessMethodBac:
        return &emrtd_access_driver_bac;
    default:
        /* Automatic is not one driver, so the caller walks the registry. */
        return NULL;
    }
}
