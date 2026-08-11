#pragma once

#include <furi.h>
#include <datetime/datetime.h>

#include "i2c_worker.h"

// Builds the human-readable report. The same text is what the screen shows
// and what lands on the SD card — a report you hand to a courier must not
// differ from the file you email afterwards.
//
// disputed: the buyer has said this is not the part they ordered, which turns
// the document from an inspection note into a reason for refusing delivery.
void report_build(
    FuriString* out,
    const I2CFoundDevice* found,
    uint8_t count,
    bool disputed,
    const DateTime* dt);
