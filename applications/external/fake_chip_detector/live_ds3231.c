#include "live_ds3231.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// Registers from the Maxim DS3231 datasheet (19-5170; Rev 10; 3/15), the
// register map in Figure 1 on page 11.
//
// This test writes NOTHING. That is deliberate and worth stating: the RTC in
// front of the user may already be keeping time they care about, and page 12
// says "the countdown chain is reset whenever the seconds register is
// written". Reading cannot disturb it — a read only latches the current time
// into a shadow copy while the clock keeps running (page 11) — so there is
// also nothing to put back on the way out.
#define DS3231_REG_SECONDS  0x00
#define DS3231_REG_TEMP_MSB 0x11

// Temperature is a 10-bit two's-complement value at 0.25 C per LSB: the
// integer part in 0x11, the fraction in the top two bits of 0x12 (page 15).
// The bottom six bits of 0x12 are fixed zero in the map on page 11, which is
// a cheap sanity check on whether these two bytes really are a temperature.
#define DS3231_TEMP_LSB_RESERVED    0x3F
#define DS3231_TEMP_SHIFT           6
#define DS3231_TEMP_MILLI_C_PER_LSB 250

// Four times a second: fast enough that no tick can slip past unseen, slow
// enough to leave the bus mostly idle.
#define DS3231_POLL_MS 250

// Three clean one-second steps. One could be a coincidence in garbage data;
// three consecutive increments of exactly one cannot be.
#define DS3231_PROOF_TICKS 3

// Bit layout of the timekeeping registers, from the same map (Figure 1, page
// 11) and the text describing it on page 12. Seconds and minutes occupy bits
// 6:0, so bit 7 is masked off rather than trusted. In the hours register bit 6
// selects 12-hour mode; bit 5 is AM/PM there (high = PM) and the second
// 10-hour bit (20-23) in 24-hour mode, which is why the same 0x20 appears
// twice below under two names — see ds3231_decode_hour.
#define DS3231_SECONDS_MASK 0x7F
#define DS3231_MINUTES_MASK 0x7F
#define DS3231_HOUR_12H_BIT 0x40
#define DS3231_HOUR_PM_BIT  0x20
#define DS3231_HOUR_10_BIT  0x10
#define DS3231_HOUR_20_BIT  0x20

// Every value in these registers is BCD (page 11), so the low nibble is the
// units digit and the high nibble the tens.
#define DS3231_BCD_UNITS_MASK 0x0F

static void ds3231_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

// The time registers are BCD (page 11), so each nibble has to be a decimal
// digit. A part returning raw binary, 0x00 or 0xFF fails this immediately,
// and that matters: it is the difference between "the clock is running" and
// "something is echoing bytes at us".
static bool ds3231_bcd_valid(uint8_t value, uint8_t max_tens) {
    return (value & DS3231_BCD_UNITS_MASK) <= 9 && (value >> 4) <= max_tens;
}

static uint8_t ds3231_bcd_to_bin(uint8_t value) {
    return (uint8_t)((value >> 4) * 10 + (value & DS3231_BCD_UNITS_MASK));
}

// Hours carry a mode bit, and the same physical bit means different things in
// each mode: in 12-hour mode bit 5 is AM/PM, in 24-hour mode it is the 20-hour
// digit (page 12). Getting this wrong would show 3 PM as 03:00.
static bool ds3231_decode_hour(uint8_t raw, uint8_t* hour_out) {
    if((raw & DS3231_BCD_UNITS_MASK) > 9) return false;
    uint8_t units = raw & DS3231_BCD_UNITS_MASK;

    if(raw & DS3231_HOUR_12H_BIT) {
        uint8_t hour = (uint8_t)(((raw & DS3231_HOUR_10_BIT) ? 10 : 0) + units);
        if(hour < 1 || hour > 12) return false;
        if(raw & DS3231_HOUR_PM_BIT) {
            if(hour != 12) hour = (uint8_t)(hour + 12);
        } else if(hour == 12) {
            hour = 0; // 12 AM is midnight
        }
        *hour_out = hour;
        return true;
    }

    uint8_t tens =
        (uint8_t)(((raw & DS3231_HOUR_20_BIT) ? 2 : 0) + ((raw & DS3231_HOUR_10_BIT) ? 1 : 0));
    uint8_t hour = (uint8_t)(tens * 10 + units);
    if(hour > 23) return false;
    *hour_out = hour;
    return true;
}

// Returns false only if the part stopped answering. `valid` says whether what
// came back actually looks like a clock.
static bool ds3231_read_time(
    const LiveTestI2c* i2c,
    uint8_t addr7,
    uint8_t* h,
    uint8_t* m,
    uint8_t* s,
    bool* valid) {
    uint8_t buf[3] = {0};
    if(!i2c->read_mem(addr7, DS3231_REG_SECONDS, buf, sizeof(buf), LIVE_TEST_TIMEOUT_MS))
        return false;

    uint8_t secs = buf[0] & DS3231_SECONDS_MASK;
    uint8_t mins = buf[1] & DS3231_MINUTES_MASK;
    *valid = ds3231_bcd_valid(secs, 5) && ds3231_bcd_valid(mins, 5) &&
             ds3231_decode_hour(buf[2], h);
    if(!*valid) return true;

    *s = ds3231_bcd_to_bin(secs);
    *m = ds3231_bcd_to_bin(mins);
    return true;
}

// The die temperature is a DS3231 feature the pin-compatible DS1307 does not
// have, so a plausible reading here is a small extra piece of evidence. It is
// shown, never used to judge: on a DS1307 those two addresses fall inside the
// battery-backed RAM and could legitimately hold anything at all. Accusing a
// part on that basis would be exactly the kind of guess this app refuses.
static bool ds3231_read_temp(const LiveTestI2c* i2c, uint8_t addr7, int32_t* milli_c) {
    uint8_t buf[2] = {0};
    if(!i2c->read_mem(addr7, DS3231_REG_TEMP_MSB, buf, sizeof(buf), LIVE_TEST_TIMEOUT_MS))
        return false;
    if(buf[1] & DS3231_TEMP_LSB_RESERVED) return false;

    int16_t raw = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    *milli_c = (int32_t)(raw >> DS3231_TEMP_SHIFT) * DS3231_TEMP_MILLI_C_PER_LSB;
    return true;
}

// Thousandths of a degree to one decimal place. The magnitude is split off
// before dividing so a reading between 0 and -1 C keeps its sign, which plain
// integer division silently drops: -0.25 C would otherwise print as "0.2".
static void ds3231_format_temp(char* out, size_t len, int32_t milli_c) {
    bool negative = milli_c < 0;
    uint32_t magnitude = (uint32_t)(negative ? -milli_c : milli_c);
    if(magnitude > 99999u) magnitude = 99999u;
    snprintf(
        out,
        len,
        "%s%u.%u",
        negative ? "-" : "",
        (unsigned)(magnitude / 1000u),
        (unsigned)(magnitude % 1000u / 100u));
}

static void ds3231_run(const LiveTestEnv* env) {
    const uint8_t addr7 = env->addr7;
    const volatile bool* stop = env->stop;
    const LiveTestI2c* i2c = env->i2c;
    const LiveTestPublish publish = env->publish;
    void* const ctx = env->ctx;

    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Reading the clock");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Nothing is written to it");
        publish(ctx, &st);

        int16_t last_sec = -1;
        uint8_t ticks = 0;
        uint8_t errors = 0;

        while(!*stop && errors < 3) {
            uint8_t hh = 0, mm = 0, ss = 0;
            bool valid = false;
            if(!ds3231_read_time(i2c, addr7, &hh, &mm, &ss, &valid)) {
                errors++;
                ds3231_delay(stop, DS3231_POLL_MS);
                continue;
            }
            errors = 0;

            memset(&st, 0, sizeof(st));
            st.phase = LiveTestPhaseRunning;

            if(!valid) {
                // Answering with something that is not a time at all. Say that
                // plainly rather than rendering the bytes as a clock.
                last_sec = -1;
                ticks = 0;
                snprintf(st.heading, sizeof(st.heading), "--");
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Not a valid BCD time");
                publish(ctx, &st);
                ds3231_delay(stop, DS3231_POLL_MS);
                continue;
            }

            // A clock proves itself by advancing by exactly one second at a
            // time. A stuck register never moves; a part improvising bytes
            // jumps around. Anything but +1 puts the count back to zero.
            if(last_sec >= 0 && ss != (uint8_t)last_sec) {
                if(ss == (uint8_t)((last_sec + 1) % 60)) {
                    if(ticks < DS3231_PROOF_TICKS) ticks++;
                } else {
                    ticks = 0;
                }
            }
            last_sec = (int16_t)ss;

            st.value = (float)ss;
            st.bar = ss;
            st.bar_max = 59;
            snprintf(st.heading, sizeof(st.heading), "%02u", ss);
            snprintf(st.unit, sizeof(st.unit), "sec");

            // The modulo is redundant - both fields were validated above - but
            // it is what proves to the compiler that two digits is the widest
            // these can print, which is what keeps the composed line inside 26
            // characters without a runtime check.
            unsigned h24 = (unsigned)hh % 24u, m60 = (unsigned)mm % 60u, s60 = (unsigned)ss % 60u;

            int32_t milli_c = 0;
            if(ds3231_read_temp(i2c, addr7, &milli_c)) {
                char die[8];
                ds3231_format_temp(die, sizeof(die), milli_c);
                snprintf(
                    st.lines[0], LIVE_TEST_LINE_LEN, "%02u:%02u:%02u  %s C", h24, m60, s60, die);
            } else {
                // Say so rather than quietly dropping the field. The die
                // thermometer is the one thing a DS3231 has that the
                // pin-compatible DS1307 does not, and both live at 0x68 with
                // the same time registers, so a DS1307 sold as a DS3231 will
                // otherwise sail through this test. It is reported, never
                // judged: on a DS1307 those two addresses are battery-backed
                // RAM and may legitimately hold anything at all.
                snprintf(
                    st.lines[0], LIVE_TEST_LINE_LEN, "%02u:%02u:%02u  no temp", h24, m60, s60);
            }

            if(ticks >= DS3231_PROOF_TICKS) st.phase = LiveTestPhasePassed;
            publish(ctx, &st);

            ds3231_delay(stop, DS3231_POLL_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        publish(ctx, &st);
        ds3231_delay(stop, LIVE_TEST_RETRY_MS);
    }
    // Nothing to tear down: this test never wrote a byte.
}

const LiveTest live_test_ds3231 = {
    .chip = "DS3231",
    .title = "DS3231 test",
    .offer = "Watch the clock run",
    .addrs = {0x68},
    .run = ds3231_run,
    .draw = NULL,
};
