#include "onewire_worker.h"

#include <furi.h>
#include <furi_hal_resources.h>
#include <one_wire/one_wire_host.h>
#include <one_wire/maxim_crc.h>

// Family codes come from Maxim/Analog application note AN937 and the parts'
// own datasheets. The family code is not a hint: it selects the command set,
// so reading a different one than the label promises is a hard fact.
typedef struct {
    uint8_t family;
    const char* name;
    const char* kind;
    OneWireRole role;
} OneWireFamily;

static const OneWireFamily onewire_families[] = {
    {0x01, "DS1990A/DS2401", "Serial number key", OneWireRoleOther},
    {0x04, "DS2404", "Clock + memory", OneWireRoleMemory},
    {0x05, "DS2405", "Addressable switch", OneWireRoleSwitch},
    {0x10, "DS18S20", "Temperature sensor", OneWireRoleTemperature},
    {0x1D, "DS2423", "RAM + counter", OneWireRoleMemory},
    {0x20, "DS2450", "4-channel ADC", OneWireRoleOther},
    {0x22, "DS1822", "Temperature sensor", OneWireRoleTemperature},
    {0x26, "DS2438", "Battery monitor", OneWireRoleOther},
    {0x28, "DS18B20", "Temperature sensor", OneWireRoleTemperature},
    {0x29, "DS2408", "8-channel switch", OneWireRoleSwitch},
    {0x2D, "DS2431", "1Kb EEPROM", OneWireRoleMemory},
    {0x3A, "DS2413", "Dual switch", OneWireRoleSwitch},
    {0x3B, "DS1825/MAX31826", "Temperature sensor", OneWireRoleTemperature},
    {0x42, "DS28EA00", "Temperature sensor", OneWireRoleTemperature},
    {0x43, "DS28EC20", "20Kb EEPROM", OneWireRoleMemory},
};

#define ONEWIRE_CMD_MATCH_ROM 0x55
#define ONEWIRE_CMD_CONVERT_T 0x44
#define ONEWIRE_CMD_READ_SCRATCHPAD 0xBE

// Datasheet worst case for a 12-bit conversion is 750 ms; the margin costs
// nothing and a truncated wait reads back a stale or garbage temperature.
#define ONEWIRE_CONVERT_MS 800

static const OneWireFamily* onewire_family_lookup(uint8_t family_code) {
    for(size_t i = 0; i < COUNT_OF(onewire_families); i++) {
        if(onewire_families[i].family == family_code) return &onewire_families[i];
    }
    return NULL;
}

// A parasitic-power sensor needs the line held high while it converts, and an
// externally powered one does not care, so driving it high suits both.
static void
    onewire_read_temperature(OneWireHost* host, OneWireDevice* dev, const volatile bool* abort) {
    if(!onewire_host_reset(host)) return;
    onewire_host_write(host, ONEWIRE_CMD_MATCH_ROM);
    onewire_host_write_bytes(host, dev->rom, sizeof(dev->rom));
    onewire_host_write(host, ONEWIRE_CMD_CONVERT_T);

    // Waited out in slices so leaving the screen is not stuck behind a
    // conversion that nobody is going to look at.
    for(uint32_t waited = 0; waited < ONEWIRE_CONVERT_MS; waited += 50) {
        if(abort && *abort) return;
        furi_delay_ms(50);
    }

    if(!onewire_host_reset(host)) return;
    onewire_host_write(host, ONEWIRE_CMD_MATCH_ROM);
    onewire_host_write_bytes(host, dev->rom, sizeof(dev->rom));
    onewire_host_write(host, ONEWIRE_CMD_READ_SCRATCHPAD);

    uint8_t scratch[9];
    onewire_host_read_bytes(host, scratch, sizeof(scratch));

    dev->measured = true;

    // A checksum is only evidence if the thing it checks could have failed it.
    // CRC8 over eight zero bytes is zero, so a scratchpad that came back as
    // nothing at all — the device stopped driving the line, the wire came out
    // mid-read — passes the check perfectly and converts to exactly 0.0 C,
    // which the screen then shows as a sensor that works. Nine zero bytes are
    // not a reading, whatever the arithmetic says about them.
    bool all_zero = true;
    for(size_t i = 0; i < sizeof(scratch); i++) {
        if(scratch[i]) {
            all_zero = false;
            break;
        }
    }

    dev->scratch_crc_ok = !all_zero && maxim_crc8(scratch, 8, 0) == scratch[8];
    if(!dev->scratch_crc_ok) return;

    int16_t raw = (int16_t)((uint16_t)scratch[0] | ((uint16_t)scratch[1] << 8));
    // The DS18S20 predates the DS18B20's 1/16 °C format and reports in halves.
    dev->temp_c = (dev->rom[0] == 0x10) ? (float)raw / 2.0f : (float)raw / 16.0f;
}

void onewire_worker_scan(OneWireScanResult* out, const volatile bool* abort) {
    furi_assert(out);
    memset(out, 0, sizeof(OneWireScanResult));

    // Read the idle line before driving it. A bus with no pull-up and a bus
    // shorted to ground both sit low, and both mean "do not trust anything
    // the search returns" — so say so instead of reporting an empty bus.
    furi_hal_gpio_init(&gpio_ibutton, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_delay_us(100);
    bool idle_high = furi_hal_gpio_read(&gpio_ibutton);
    furi_hal_gpio_init(&gpio_ibutton, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    if(!idle_high) {
        out->state = OneWireBusShorted;
        return;
    }

    OneWireHost* host = onewire_host_alloc(&gpio_ibutton);
    onewire_host_start(host);

    if(!onewire_host_reset(host)) {
        out->state = OneWireBusEmpty;
    } else {
        out->state = OneWireBusOk;
        onewire_host_reset_search(host);

        uint8_t rom[8];
        while(onewire_host_search(host, rom, OneWireHostSearchModeNormal)) {
            if(out->count >= ONEWIRE_MAX_FOUND) {
                out->overflow = true;
                break;
            }
            OneWireDevice* dev = &out->found[out->count++];
            memcpy(dev->rom, rom, sizeof(rom));
            dev->crc_ok = maxim_crc8(rom, 7, 0) == rom[7];

            const OneWireFamily* f = onewire_family_lookup(rom[0]);
            if(f) {
                dev->name = f->name;
                dev->kind = f->kind;
                dev->role = f->role;
            } else {
                dev->role = OneWireRoleOther;
            }
        }

        // Measure only after the search has finished: a conversion resets the
        // bus, which would restart the search from the beginning.
        for(uint8_t i = 0; i < out->count && !(abort && *abort); i++) {
            OneWireDevice* dev = &out->found[i];
            if(dev->crc_ok && dev->role == OneWireRoleTemperature) {
                onewire_read_temperature(host, dev, abort);
            }
        }
    }

    onewire_host_stop(host);
    onewire_host_free(host);
    furi_hal_gpio_init(&gpio_ibutton, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}
