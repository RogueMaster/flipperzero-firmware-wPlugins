#include "report.h"
#include "chip_db.h"

// Written to be read aloud at a front door, by someone who has never heard of
// I2C. Plain statement first, then why the evidence cannot be faked, and only
// at the very bottom the register values an engineer would want.

// The nothing-answered document. Deliberately refuses to conclude: everything
// here is either a measurement or a fact about how these parts are built, and
// the one sentence a reader will act on says plainly that none of it is
// evidence the chip is broken.
static void report_silence(FuriString* out, const SilentDiagnosis* s) {
    furi_string_cat_str(out, "NOTHING ANSWERED ON THE I2C BUS\n\n");

    switch(s->bus.health) {
    case I2CBusStuckLow:
        furi_string_cat_str(
            out,
            "A data line is held low. That is a wiring fault or a chip that has hung, "
            "and it has to be fixed before anything can be said about the part.\n\n");
        break;
    case I2CBusFloating:
        furi_string_cat_str(
            out,
            "Neither data line is pulled up, so the module is either unpowered or not "
            "connected. Nothing can be concluded about the chip from this.\n\n");
        break;
    default:
        furi_string_cat_str(
            out,
            "The bus itself was electrically healthy: the module had power and its "
            "pull-up resistors were present on both lines. Something is connected and "
            "it did not answer.\n\n");
        break;
    }

    if(s->pad_measured && s->pad_labels) {
        const char* level = s->pad_level == I2CPadHigh ? "HIGH" :
                            s->pad_level == I2CPadLow  ? "LOW" :
                                                         "floating";
        furi_string_cat_printf(out, "The pin marked %s measured %s.\n\n", s->pad_labels, level);

        if(s->pad_held) {
            furi_string_cat_printf(
                out,
                "Pulling that pin %s did not move it, so it is tied %s on the board itself. "
                "The module was built this way and no rewiring can change it.\n\n",
                s->pad_wanted_high ? "up" : "down",
                s->pad_wanted_high ? "low" : "high");
        }
    }

    furi_string_cat_str(
        out,
        "WHY THIS IS NOT PROOF OF A FAULT\n"
        "Many sensors have a pin that chooses which kind of connection they use. Set one "
        "way the chip talks I2C; set the other it talks SPI or a serial line instead, and "
        "then it has no I2C address at all and cannot answer, however healthy it is. That "
        "pin is often decided by the board rather than by the buyer. An empty scan is "
        "therefore a reason to look closer, not a reason to call the part defective.\n\n");
}

void report_build(
    FuriString* out,
    const I2CFoundDevice* found,
    uint8_t count,
    bool disputed,
    const DateTime* dt,
    const SilentDiagnosis* silent) {
    furi_string_reset(out);

    if(count == 0 && silent) {
        report_silence(out, silent);
        furi_string_cat_printf(
            out,
            "Checked %04u-%02u-%02u %02u:%02u with Fake Chip Detector on a Flipper Zero.\n\n"
            "--- TECHNICAL DETAIL ---\n"
            "Bus: I2C, 100 kHz. Swept every 7-bit address, 0x08 to 0x77.\n"
            "SCL pull-up: %s   SDA pull-up: %s   Lines shorted: %s\n",
            dt->year,
            dt->month,
            dt->day,
            dt->hour,
            dt->minute,
            silent->bus.scl_ok ? "yes" : "no",
            silent->bus.sda_ok ? "yes" : "no",
            silent->bus.shorted ? "yes" : "no");
        return;
    }

    if(disputed) {
        furi_string_cat_str(
            out,
            "REASON FOR REFUSING THIS ITEM\n"
            "Show or read this to the seller or courier.\n\n");
    } else {
        furi_string_cat_str(out, "CHIP INSPECTION REPORT\n\n");
    }

    for(uint8_t i = 0; i < count; i++) {
        const I2CFoundDevice* dev = &found[i];
        const char* name = dev->ident.chip ? dev->ident.chip->name : NULL;
        const char* kind = dev->ident.chip ? dev->ident.chip->kind : NULL;

        if(name && kind) {
            furi_string_cat_printf(out, "The chip inside this module is a %s.\n", name);
            furi_string_cat_printf(out, "That part is a %s.\n\n", kind);
        } else {
            furi_string_cat_str(
                out, "A chip answered, but it does not identify itself as any known part.\n\n");
        }

        switch(dev->ident.verdict) {
        case VerdictGenuine:
            furi_string_cat_printf(out, "Its factory ID matches a real %s exactly.\n\n", name);
            break;
        case VerdictWrongChip:
            furi_string_cat_printf(
                out,
                "Part of its factory ID is wrong. A real %s answers with different values, "
                "so this is not that part.\n\n",
                name ? name : "chip");
            break;
        case VerdictDetectedNoId:
            furi_string_cat_str(
                out,
                "This type of chip carries no factory ID, so only its presence could be "
                "confirmed.\n\n");
            break;
        case VerdictNoAnswer:
            furi_string_cat_str(
                out, "The chip acknowledged its address but returned no data.\n\n");
            break;
        default:
            furi_string_cat_str(out, "The ID it reported matches no chip known to this tool.\n\n");
            break;
        }
    }

    if(disputed) {
        furi_string_cat_str(out, "This is not the part that was ordered.\n\n");
    }

    furi_string_cat_str(
        out,
        "HOW THIS WAS CHECKED\n"
        "Every chip of this kind has an identity number written into the silicon at the "
        "factory. It is read-only: no software and no seller can change it. The chip was "
        "asked for that number over its standard data connection and the answer is above. "
        "Anyone can repeat this test with the same free tool and get the same result.\n\n");

    furi_string_cat_printf(
        out,
        "Checked %04u-%02u-%02u %02u:%02u with Fake Chip Detector on a Flipper Zero.\n\n",
        dt->year,
        dt->month,
        dt->day,
        dt->hour,
        dt->minute);

    furi_string_cat_str(out, "--- TECHNICAL DETAIL ---\nBus: I2C, 100 kHz\n");

    for(uint8_t i = 0; i < count; i++) {
        const I2CFoundDevice* dev = &found[i];
        furi_string_cat_printf(
            out,
            "addr 0x%02X %s %s\n",
            dev->addr,
            dev->ident.chip ? dev->ident.chip->name : "UNKNOWN",
            chip_verdict_str(dev->ident.verdict));
        for(uint8_t r = 0; r < dev->ident.read_count; r++) {
            const IdReadResult* rr = &dev->ident.reads[r];
            uint8_t rdigits = rr->reg16 ? 4 : 2;
            uint8_t digits = rr->wide ? 4 : 2;
            if(!rr->read_ok) {
                furi_string_cat_printf(out, " reg 0x%0*X read FAILED\n", rdigits, rr->reg);
            } else if(rr->has_expected) {
                furi_string_cat_printf(
                    out,
                    " reg 0x%0*X = 0x%0*X (exp 0x%0*X) %s\n",
                    rdigits,
                    rr->reg,
                    digits,
                    rr->actual,
                    digits,
                    rr->expected,
                    rr->match ? "OK" : "MISMATCH");
            } else {
                furi_string_cat_printf(
                    out, " reg 0x%0*X = 0x%02X\n", rdigits, rr->reg, rr->actual);
            }
        }
        if(dev->ident.chip && dev->ident.chip->note) {
            furi_string_cat_printf(out, " note: %s\n", dev->ident.chip->note);
        }
    }
    if(count == 0) furi_string_cat_str(out, "No devices found\n");
}

// One place decides how a tenth-of-a-degree reading is spelled, because the
// obvious way is wrong. Dividing tenths by ten and printing it with %d loses
// the sign everywhere between -0.9 and -0.1 C: C truncates toward zero, so the
// whole part of -5 tenths is 0, and "0" has no minus to print. A sensor in a
// freezer would report half a degree above zero.
static void report_cat_temp(FuriString* out, float temp_c) {
    int tenths = (int)(temp_c * 10.0f);
    int mag = tenths < 0 ? -tenths : tenths;
    furi_string_cat_printf(out, "%s%d.%d", tenths < 0 ? "-" : "", mag / 10, mag % 10);
}

// Every part in the 1-Wire temperature families this app decodes measures
// -55..+125 C. A number outside that is not a temperature, and reading "it
// works" off it would be exactly the kind of claim this app exists to stop.
#define OW_TEMP_MIN_TENTHS (-550)
#define OW_TEMP_MAX_TENTHS 1250

void report_build_onewire(FuriString* out, const OneWireScanResult* res, const DateTime* dt) {
    furi_string_reset(out);
    furi_string_cat_str(out, "1-WIRE INSPECTION REPORT\n\n");

    if(res->count == 0) {
        switch(res->state) {
        case OneWireBusShorted:
            furi_string_cat_str(
                out,
                "Nothing could be read. The data line stayed low the whole time, which means "
                "it is shorted to ground or the 4.7k pull-up resistor is missing. No "
                "conclusion about any part can be drawn from this.\n\n");
            break;
        case OneWireBusEmpty:
            furi_string_cat_str(
                out,
                "No device answered. Nothing on the bus responded to the signal that every "
                "1-Wire part must answer, so either nothing is connected to the data wire or "
                "it has no power.\n\n");
            break;
        default:
            // A presence pulse came back and then the ID search returned
            // nothing. Something is on the wire; saying "no device" would
            // throw away the one fact this run did establish.
            furi_string_cat_str(
                out,
                "Something on the bus answered the first signal, but then gave no ID when "
                "asked for one. A part is connected; it did not identify itself.\n\n");
            break;
        }
    }

    for(uint8_t i = 0; i < res->count; i++) {
        const OneWireDevice* dev = &res->found[i];

        if(dev->name && dev->kind) {
            furi_string_cat_printf(out, "The part on this bus is a %s.\n", dev->name);
            furi_string_cat_printf(out, "That part is a %s.\n", dev->kind);
        } else {
            furi_string_cat_printf(
                out,
                "A device answered with family code 0x%02X, which is not a part this tool "
                "knows.\n",
                dev->rom[0]);
        }

        if(!dev->crc_ok) {
            furi_string_cat_str(
                out,
                "Its ID failed the checksum carried inside the ID itself, so the number was "
                "misread and nothing above it should be relied on. Check the wiring and read "
                "it again.\n\n");
            continue;
        }

        int tenths = (int)(dev->temp_c * 10.0f);
        if(dev->measured && dev->scratch_crc_ok && tenths >= OW_TEMP_MIN_TENTHS &&
           tenths <= OW_TEMP_MAX_TENTHS) {
            furi_string_cat_str(out, "It was asked for a temperature and answered ");
            report_cat_temp(out, dev->temp_c);
            furi_string_cat_str(
                out, " C, so the measuring part of it works and not only the ID.\n\n");
        } else if(dev->measured && dev->scratch_crc_ok) {
            furi_string_cat_str(
                out,
                "It answered a temperature outside the range this kind of part can measure, "
                "so the reading proves nothing about it.\n\n");
        } else if(dev->measured) {
            furi_string_cat_str(
                out,
                "It answered a measurement, but the data failed its own checksum, so the "
                "number could not be trusted and is not printed here.\n\n");
        } else if(dev->role == OneWireRoleTemperature) {
            furi_string_cat_str(
                out, "No temperature came back from it, so only its identity was checked.\n\n");
        } else {
            furi_string_cat_str(
                out,
                "It is present and its ID checks out. This kind of part has nothing to "
                "measure, so that is as far as the check goes.\n\n");
        }
    }

    // The flag exists to be reported. A list of eight that was really a bus of
    // twelve reads as a complete inventory, and the reader has no way to know.
    if(res->overflow) {
        furi_string_cat_printf(
            out,
            "There were more devices on this bus than this tool can list. The %u above are "
            "the ones it reached; this is not a complete inventory.\n\n",
            res->count);
    }

    furi_string_cat_str(
        out,
        "HOW THIS WAS CHECKED\n"
        "Every 1-Wire part carries a 64-bit number burned into it at the factory, and the "
        "lowest byte of that number says which kind of part it is. This tool read the "
        "number over the single data wire, checked it against the checksum built into it, "
        "and decoded it.\n\n"
        "Read this next part before relying on the one above. A 1-Wire number can be "
        "copied: any microcontroller can be programmed to answer with someone else's, so "
        "this document says which part answered, not who made it. What it does catch is "
        "the substitution that actually happens - a cheaper part of a different kind sold "
        "under a popular name - because a genuine part announces its own kind and has no "
        "way to announce another. Anyone can repeat this test with the same free tool and "
        "get the same result.\n\n");

    furi_string_cat_printf(
        out,
        "Checked %04u-%02u-%02u %02u:%02u with Fake Chip Detector on a Flipper Zero.\n\n",
        dt->year,
        dt->month,
        dt->day,
        dt->hour,
        dt->minute);

    furi_string_cat_str(out, "--- TECHNICAL DETAIL ---\nBus: 1-Wire, header pin 17\n");

    switch(res->state) {
    case OneWireBusShorted:
        furi_string_cat_str(out, "bus: held low, search not attempted\n");
        break;
    case OneWireBusEmpty:
        furi_string_cat_str(out, "bus: no presence pulse\n");
        break;
    default:
        break;
    }

    for(uint8_t i = 0; i < res->count; i++) {
        const OneWireDevice* dev = &res->found[i];
        // 16 hex digits with no separators, the same shape the screen shows:
        // it is an identifier to compare against a listing, not prose.
        furi_string_cat_str(out, "rom ");
        for(uint8_t b = 0; b < 8; b++) {
            furi_string_cat_printf(out, "%02X", dev->rom[b]);
        }
        furi_string_cat_printf(out, " %s\n", dev->name ? dev->name : "UNKNOWN");
        furi_string_cat_printf(
            out, " family 0x%02X, ROM CRC %s\n", dev->rom[0], dev->crc_ok ? "OK" : "BAD");
        if(dev->measured && dev->scratch_crc_ok) {
            furi_string_cat_str(out, " temp ");
            report_cat_temp(out, dev->temp_c);
            furi_string_cat_str(out, " C, scratchpad CRC OK\n");
        } else if(dev->measured) {
            // temp_c is left at zero when the scratchpad fails its checksum,
            // and printing that zero next to a BAD would read as 0.0 C.
            furi_string_cat_str(out, " scratchpad CRC BAD, no reading\n");
        }
    }

    if(res->overflow) {
        furi_string_cat_printf(
            out, "more than %u devices on the bus; the list above stops there\n", res->count);
    }
    if(res->count == 0) furi_string_cat_str(out, "No devices found\n");
}
