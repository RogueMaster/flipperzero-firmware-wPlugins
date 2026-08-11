#include "report.h"
#include "chip_db.h"

// Written to be read aloud at a front door, by someone who has never heard of
// I2C. Plain statement first, then why the evidence cannot be faked, and only
// at the very bottom the register values an engineer would want.

void report_build(
    FuriString* out,
    const I2CFoundDevice* found,
    uint8_t count,
    bool disputed,
    const DateTime* dt) {
    furi_string_reset(out);

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
