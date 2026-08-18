#pragma once

#include <furi.h>
#include <datetime/datetime.h>

#include "i2c_worker.h"
#include "onewire_worker.h"

// Builds the human-readable report. The same text is what the screen shows
// and what lands on the SD card — a report you hand to a courier must not
// differ from the file you email afterwards.
//
// What the app managed to establish when nothing answered at all. NULL on the
// normal path. The person who most needs a document is the one whose chip
// never spoke -- they are the one being told to accept or refuse a parcel with
// no evidence either way -- and until this existed they were the only user who
// could not save one.
typedef struct {
    I2CBusCheck bus;
    bool pad_measured; // the user walked a wire onto a mode pad
    uint8_t pad_level; // I2CPadLevel
    const char* pad_labels; // the family of labels they said they were on
    // A weak pull the other way was tried and lost, so the pad is tied in
    // copper. For a buyer that is the whole answer: the seller shipped a board
    // configured off the I2C bus, and no amount of rewiring at a counter will
    // change it.
    bool pad_held;
    bool pad_wanted_high; // the level I2C needed, for the sentence above
} SilentDiagnosis;

// disputed: the buyer has said this is not the part they ordered, which turns
// the document from an inspection note into a reason for refusing delivery.
void report_build(
    FuriString* out,
    const I2CFoundDevice* found,
    uint8_t count,
    bool disputed,
    const DateTime* dt,
    const SilentDiagnosis* silent);

// A separate document, not another section of the one above. What gives the
// I2C report its force is the paragraph saying the factory ID is read-only and
// no seller can change it — and that sentence is simply untrue of a 1-Wire ROM
// code, which any microcontroller can replay. Sharing the paragraph would put
// the app's one real overclaim into the page a user hands to a courier.
//
// There is no disputed flavour here on purpose: the 1-Wire screen never asks
// what the buyer ordered, and it does not need to. The decoded part name and
// its family code are the refusal evidence by themselves.
void report_build_onewire(FuriString* out, const OneWireScanResult* res, const DateTime* dt);
