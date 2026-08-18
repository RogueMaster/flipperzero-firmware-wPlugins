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
