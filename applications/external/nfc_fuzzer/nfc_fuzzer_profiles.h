#pragma once

#include "nfc_fuzzer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A single generated test case. */
typedef struct {
    uint8_t data[NFC_FUZZER_MAX_PAYLOAD_LEN];
    uint8_t data_len;
} NfcFuzzerTestCase;

/**
 * Initialize / reset a profile's internal generator state.
 * Must be called before first nfc_fuzzer_profile_next().
 */
void nfc_fuzzer_profile_init(NfcFuzzerProfile profile, NfcFuzzerStrategy strategy);

/**
 * Generate the next test case for a profile + strategy.
 * @param profile  Selected fuzz profile.
 * @param strategy Selected mutation strategy.
 * @param index    Current test case index (0-based).
 * @param out      Output test case structure.
 * @return true if a test case was generated, false if exhausted.
 */
bool nfc_fuzzer_profile_next(
    NfcFuzzerProfile profile,
    NfcFuzzerStrategy strategy,
    uint32_t index,
    NfcFuzzerTestCase* out);

/**
 * Return the total number of test cases for a profile + strategy combination.
 * Returns UINT32_MAX if unbounded (random).
 */
uint32_t nfc_fuzzer_profile_total_cases(NfcFuzzerProfile profile, NfcFuzzerStrategy strategy);

#ifdef __cplusplus
}
#endif
